
use strict;
use ExtUtils::MakeMaker;
use ExtUtils::Embed;

print "Makefile.PL: creating xsinit.c\n";
xsinit('xsinit.c');

WriteMakefile(
    NAME		=> 'CGI::SpeedyCGI',
    OBJECT		=> 'copybuf.o util.o start_perl.o opts.o queue.o
			    speedy.o xsinit.o',
    VERSION_FROM	=> 'lib/CGI/SpeedyCGI.pm',
    MAP_TARGET		=> 'speedy',
);

package MY;

use ExtUtils::Embed;

sub postamble {
    '
all :: speedy speedyhandler

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

#sub install {
#    '
#install: speedy speedyhandler
#	rm -f $(INSTALLBIN)/speedyhandler $(INSTALLBIN)/speedy
#	cp speedy $(INSTALLBIN)/speedy
#	ln $(INSTALLBIN)/speedy $(INSTALLBIN)/speedyhandler
#    ';
#}

sub makeaperl {
    my $ldopts = ldopts('-std');
    my $ccopts = ccopts();
    "
speedy: \$(O_FILES)
	\$(CC) $ccopts -o speedy \$(O_FILES) $ldopts
    ";
}
