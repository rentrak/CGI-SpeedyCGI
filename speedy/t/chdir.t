
# Test #1: if script cd's elsewhere, it should come back on the next run.

# Test #2: If the perl process does a chdir then hits the maxruns limit
# it should go back to the original directory before exec'ing itself.

# Test #3 same as #1, but cd to /tmp in between runs.  The backend
# shoudl track the chdir

# Tests 4-6, similar to 1-3, but start from a path where the parent
# is unreabable, meaning getcwd will fail on some oses.  The backend
# may not be able to get to the right dir in 4/5, so don't check that.

# Tests 7-9, same as 4-6, but with current directory mode 0, which makes
# stat(".") fail.

print "1..9\n";

# Test 1
my $scr = 't/scripts/chdir';

use strict;
use vars qw($start);
$start = `pwd`;
chop $start;

sub doit { my($maxruns, $cdto, $pids_only) = @_;
    utime time, time, "$start/$scr";
    sleep 1;
    my(@spdir, @pid);
    my $orig_dir = $start;
    for (my $i = 0; $i < 2; ++$i) {
	my $cmd = "$ENV{SPEEDY} -- -r$maxruns $start/$scr";
	open(F, "$cmd |");
	chop($spdir[$i] = <F>);
	chop($pid[$i] = <F>);
	close(F);
	sleep 1;
	chdir($cdto) if $cdto;
    }
    #print STDERR "pid=", join(',', @pid), " spdir=", join(',', @spdir), "\n";
    my $ok = ($pid[0] == $pid[1] && $pid[0]);
    unless ($pids_only) {
	$ok = $ok && ($cdto || $orig_dir) eq $spdir[1];
    }
    print $ok ? "ok\n" : "failed\n";
}

&doit(2);
&doit(1);
&doit(2, "/tmp");

chdir $start;
my $TMPDIR = "/tmp/unreadable$$";
mkdir $TMPDIR, 0777;
mkdir "$TMPDIR/x", 0777;
chdir "$TMPDIR/x";
chmod 0333, $TMPDIR;
&doit(2, undef, 1);
&doit(1, undef, 1);
&doit(2, "/tmp");

chdir "$TMPDIR/x";
chmod 0, ".";
&doit(2, undef, 1);
&doit(1, undef, 1);
&doit(3, "/tmp");

rmdir "$TMPDIR/x";
rmdir $TMPDIR;
