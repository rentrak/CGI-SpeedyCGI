#
# See if mod_speedycgi leaks memory.  Do 100 requests to stabilize the memory
# usage, get the amount of memory in use, then do another 200 requests and
# see if the usage goes up. 
#
# This test only works on linux.
#

use lib 't';
use ModTest;

my $scr = 'speedy/pid';

ModTest::test_init(60, [$scr]);

my $pid = ModTest::get_httpd_pid;

sub mem_used {
    my $val;
    open(F, "</proc/$pid/status") || return 0;
    while (<F>) {
	next unless /VmSize/;
	$val = (split)[1];
	last;
    }
    return $val;
}

sub do_requests { my $times = shift;
    ModTest::http_get("/$scr") while ($times--);
}

if (&mem_used) {
    print "1..1\n";
    &do_requests(100);
    my $mem1 = &mem_used;
    &do_requests(200);
    my $mem2 = &mem_used;
    ## print STDERR "mem1=$mem1 mem2=$mem2\n";
    if ($mem2 <= $mem1) {
	print "ok\n";
    } else {
	print "failed\n";
    }
} else {
    print "1..0\n";
}
