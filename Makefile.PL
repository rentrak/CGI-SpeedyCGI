
use strict;
use ExtUtils::MakeMaker;
use lib 'src';
use SpeedyMake qw(%write_makefile_common);

WriteMakefile(
    NAME		=> 'CGI::SpeedyCGI',
    VERSION_FROM	=> 'src/SpeedyCGI_src.pm',
    EXE_FILES		=> [qw(speedy/speedy speedy_backend/speedy_backend)],
    CONFIGURE		=> \&get_dir,
    PM			=> {'src/SpeedyCGI.pm'=>'$(INST_LIBDIR)/SpeedyCGI.pm'},
    MAN1PODS		=> {},
    MAN3PODS		=> {},
    dist		=> {DIST_DEFAULT => 'mydist'},
    realclean		=> {FILES => 'README README.html'},
    %write_makefile_common
);

# Find out which directories to make.  Exclude mod_speedycgi if no apxs cmd
sub get_dir {
    my @list = qw(src speedy_backend speedy);

    if (&do_apache) {
	unshift(@list, 'mod_speedycgi');
    }
    return {DIR=>\@list};
}

sub do_apache {
    use vars qw($do_apache);
    if (!defined($do_apache)) {
	system('apxs -q CC >/dev/null 2>&1');
	if ($?) {
	    print "Skipping mod_speedycgi because the 'apxs' command ";
	    print "wasn't found in your path.\n"; 
	    $do_apache = 0;
	} else {
	    $do_apache = 1;
	}
    }
    return $do_apache;
}

package MY;

use ExtUtils::Embed;

sub postamble {
    my $extra = SpeedyMake->optdefs_cmds('src');
    if (&main::do_apache) {
	$extra .= "\ninstall ::\n\tcd mod_speedycgi; make install\n\n";
    }
'
mydist: README README.html tardist

README: src/SpeedyCGI.pm
	(cd src; perl -e "use Pod::Text; pod2text(\"-80\")" < SpeedyCGI.pm >../README)
	chmod -R u+w,go-w,go+r .

README.html: src/SpeedyCGI.pm
	(cd src; pod2html SpeedyCGI.pm >../README.html; rm -f pod2html*)

rpm: install README README.html
	./util/build_rpms $(VERSION) $(INSTALLBIN) $(INSTALLSITELIB)

clean ::
	rm -f *.rpm

' . $extra;
}
