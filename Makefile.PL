
use strict;
use ExtUtils::MakeMaker;
use ExtUtils::Embed;

print "Makefile.PL: creating xsinit.c\n";
xsinit('xsinit.c');

WriteMakefile(
    NAME		=> 'CGI::SpeedyCGI',
    OBJECT		=> 'copybuf.o util.o start_perl.o opts.o
			    speedy.o xsinit.o speedy_poll.o',
    VERSION_FROM	=> 'lib/CGI/SpeedyCGI.pm',
    MAP_TARGET		=> 'speedy',
    INC			=> '-I./libspeedy',
    DEFINE		=> '-DSPEEDY_BIN=\"$(INSTALLBIN)/speedy\"',
);

package MY;

use ExtUtils::Embed;

sub postamble {
    '
all :: speedy speedyhandler README.html README apache/README.html apache/README

README: lib/CGI/SpeedyCGI.pm
	(cd lib/CGI; pod2text SpeedyCGI.pm >../../README)

README.html: lib/CGI/SpeedyCGI.pm
	(cd lib/CGI; pod2html SpeedyCGI.pm >../../README.html; rm -f pod2html*)

apache/README: apache/mod_speedycgi.pod
	(cd apache; pod2text mod_speedycgi.pod >README)

apache/README.html: apache/mod_speedycgi.pod
	(cd apache; pod2html mod_speedycgi.pod >README.html; rm -f pod2html*)

speedyhandler: speedy
	rm -f speedyhandler; ln speedy speedyhandler

clean ::
	rm -f speedyhandler xsinit.c

install :: speedy speedyhandler
	rm -f $(INSTALLBIN)/speedyhandler $(INSTALLBIN)/speedy
	cp speedy $(INSTALLBIN)/speedy
	ln $(INSTALLBIN)/speedy $(INSTALLBIN)/speedyhandler
    ';
}

sub makeaperl {
    my $ldopts = ldopts('-std');
    my $ccopts = ccopts();
    "
SPEEDYLIB=./libspeedy/blib/arch/auto/libspeedy/libspeedy.a

\$(SPEEDYLIB): libspeedy/*.c libspeedy/*.h
	cd libspeedy; perl Makefile.PL; make

speedy: \$(O_FILES) \$(SPEEDYLIB)
	rm -f speedy; \$(CC) $ccopts -o speedy \$(O_FILES) \$(SPEEDYLIB) $ldopts
    ";
}
