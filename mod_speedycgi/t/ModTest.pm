
# Useful routines used by the test scripts

package ModTest;

use strict;
use Socket;
use Cwd;

my $srvroot	= '/tmp/mod_speedycgi_test';
my $srvport	= 8529;

sub mod_dir {
    &Cwd::cwd;
}

sub docroot {
    &mod_dir . '/t/docroot';
}

sub get_httpd_pid {
    my $pid = `cat $srvroot/httpd.pid 2>/dev/null`;
    chop $pid;
    return ($pid && kill(0, $pid)) ? $pid : 0;
}

sub write_conf { my $extracfg = shift; 
    my($mod_dir, $docroot, $grp) = (&mod_dir, &docroot, $));
    $grp =~ s/\s.*//;
    my $module = $ENV{SPEEDY_MODULE}
	|| "$mod_dir/mod_speedycgi.so";
    my $backend = $ENV{SPEEDY_BACKENDPROG}
	|| "$mod_dir/../speedy_backend/speedy_backend";

    foreach my $f ($module, $backend) {
	die "Cannot locate ${f}: $!\n" unless -f $f;
    }

    # Can't run apache as user 0 (root)
    my $user = $> ? "#$>" : 'nobody';

    my $cfg = "
	ServerRoot		$srvroot
	User			$user
	Group			#$grp
	Port			$srvport
	ServerName		localhost
	DocumentRoot		$docroot
	ErrorLog		error_log
	PidFile			httpd.pid
	LockFile		lock_file
	AccessConfig		/dev/null
	ResourceConfig		/dev/null
	ScoreBoardFile		/dev/null
	Options			+ExecCGI
	LoadModule		speedycgi_module $module
	AddModule		mod_speedycgi.c
	SpeedyBackendProg	$backend
	<Directory $docroot/speedy>  
	DefaultType		 speedycgi-script
	</Directory>		 
	<IfModule mod_mime.c>
	TypesConfig /dev/null
	</IfModule>
    ";
    $cfg .= $extracfg if (defined($extracfg));
    $cfg =~ s/\n\s+/\n/g;

    mkdir($srvroot, 0777);
    open(F, ">$srvroot/httpd.conf") || die;
    print F $cfg;
    close(F);
}

sub find_httpd {
    my $x = `apxs -q SBINDIR` . '/httpd';
    return -x $x ? $x : 'httpd';
}

sub abort_test {
    my $reason = 'Cannot start httpd, skipping test';
    $| = 1;
    print "1..1\nok # skip $reason\n";
    print STDERR "$reason\n";
    exit;
};

sub start_httpd {
    &write_conf(@_);
    my $pid = &get_httpd_pid;
    if ($pid && kill(0, $pid)) {
	kill(9, $pid);
	sleep 1;
    }
    if (fork == 0) {
	close(STDOUT);
	close(STDERR);
	if (fork == 0) {
	    open(STDOUT, ">>$srvroot/httpd.stdout");
	    open(STDERR, ">&STDOUT");
	    $| = 1;
	    print 'Startup ', scalar localtime, "\n";
	    my $httpd = &find_httpd;
	    eval {exec(split(/ /, "$httpd -X -d $srvroot -f httpd.conf"))};
	    print STDERR "cannot exec httpd: $!\n";
	    die;
	}
	exit 0;
    }
    wait;
    for (my $tries = 4; $tries; --$tries) {
	my $test = '';
	eval {
	    local $SIG{ALRM} = sub { die "alarm clock restart" };
	    alarm 2;
	    $test = html_get('/htmltest');
	    alarm 0;
	};
	# print STDERR "debug=$test\n";
	return if $test =~ /html test/i;
	sleep(1);
    }
    &abort_test;
}

sub http_get { my $relurl = shift;
    my $paddr = sockaddr_in($srvport, inet_aton('localhost'));
    socket(SOCK, PF_INET, SOCK_STREAM, getprotobyname('tcp'));
    return '' unless connect(SOCK, $paddr);
    my $oldfh = select SOCK;
    $| = 1;
    print "GET $relurl HTTP/1.0\n\n";
    select $oldfh;
    my @ret = <SOCK>;
    close(SOCK);
    return wantarray ? @ret : join('', @ret);
}

sub html_get { 
    my @lines = &http_get(@_);
    if (@lines) {
	while ($lines[0] =~ /\S/) { shift @lines }
	shift @lines;
	return wantarray ? @lines : join('', @lines);
    }
    return '';
}

sub touchem { my $scripts = shift;
    my $docroot = &docroot;
    foreach my $scr (@{$scripts || []}) {
	$scr = "$docroot/$scr";
	die "$scr: $!\n" unless -f $scr;
	utime time, time, $scr;
    }
}

sub set_alarm { my $secs = shift;
    $SIG{ALRM} = sub { print "failed\n"; exit };
    alarm($secs);
}

sub test_init { my($timeout, $scripts, $extracfg) = @_;
    &set_alarm($timeout);
    &touchem($scripts);
    &start_httpd($extracfg);
}

END {
    my $pid = &get_httpd_pid;
    kill(9, $pid) if ($pid);
}

1;
