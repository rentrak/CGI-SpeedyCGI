
# Test the shutdown handler method in the SpeedyCGI module.

print "1..4\n";

my $testf = "/tmp/speedy.shutdown_done.$$";
my $scr = 't/scripts/shutdown';
unlink($testf);

utime time, time, $scr;
sleep 1;

sub run { my $which = shift;
    my $val = `$ENV{SPEEDY} -- -r2 t/scripts/shutdown $testf $which`;
    sleep 1;
    chomp $val;
    return $val;
}

# The shutdown script that we run should create $testf when it
# shuts down.
#
# Run twice by setting the maximum number of runs.
# After the first run, the file should not exist,
# but after the second run, it should exist.
#
for (my $i = 0; $i < 2; $i++) {
    if (&run($i) > 0 && ! -f $testf && &run($i) > 0 && -f $testf) {
	print "ok\n"
    } else {
	print "failed\n";
    }
    unlink $testf;
}

if (&run(2) > 0 && -f $testf) {
    print "ok\n";
} else {
    print "failed\n";
}
unlink $testf;

if (!&run(3) && -f $testf) {
    print "ok\n";
} else {
    print "failed\n";
}
unlink $testf;
