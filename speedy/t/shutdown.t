
# Test the shutdown handler method in the SpeedyCGI module.

print "1..2\n";

my $testf = "/tmp/speedy.shutdown_done.$$";
my $scr = 't/scripts/shutdown';
unlink($testf);

utime time, time, $scr;
sleep 1;

sub run {
    my $ok = `$ENV{SPEEDY} -- -r2 t/scripts/shutdown $testf` =~ /\d/;
    sleep 1;
    return $ok
}

# The shutdown script that we run should create $testf when it
# shuts down.
#
# Run twice by setting the maximum number of runs.
# After the first run, the file should not exist,
# but after the second run, it should exist.
#
if (&run && ! -f $testf) {
    print "ok\n";
    if (&run) {
	print -f $testf ? "ok\n" : "failed\n";
    }
} else {
    print "failed\nfailed\n";
}
unlink $testf;
