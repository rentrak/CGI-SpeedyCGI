
# Tests the shutdown handler feature.

print "1..2\n";

my $sp = './speedy';

my $testf = "/tmp/speedy.shutdown_done.$$";
my $scr = 't/scripts/shutdown';
unlink($testf);

utime time, time, $scr;

sub run {
    `$sp -- -r2 t/scripts/shutdown $testf`
}

# Run twice by setting the maximum number of runs.
# After the first run, the file should not exist,
# but after the second run, it should exist.
&run;
sleep 1;
if (-f $testf) {
    print "failed\nfailed\n";
} else {
    print "ok\n";
    &run;
    sleep 1;
    print -f $testf ? "ok\n" : "failed\n";
}
unlink $testf;
