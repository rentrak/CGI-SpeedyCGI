
my $sp = './speedy';

print "1..2\n";

# Test 1
my $str = "hello_world";
my $line = `$sp t/scripts/basic.1 $str`;
print ($line eq $str ? "ok\n" : "failed\n");

# Test 2
my $numruns = 4;
my $scr = 't/scripts/basic.2';
my $num;
utime time, time, $scr;
for (my $i = $numruns; $i > 0; --$i) {
    sleep 2;
    $num = `$sp $scr`;
}
print ($num == $numruns ? "ok\n" : "failed\n");
