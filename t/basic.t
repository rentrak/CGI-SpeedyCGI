
my $sp = './speedy';

print "1..2\n";

# Test 1
my $str = "hello_world";
my $line = `$sp t/scripts/basic.1 $str`;
print ($line eq $str ? "ok\n" : "failed\n");

# Test 2
my $scr = 't/scripts/basic.2';
my $num;
utime time, time, $scr;
sleep 2;
for (my $i = 9; $i > 0; --$i) {
    $num = `$sp $scr`;
}
print ($num == 9 ? "ok\n" : "failed\n");
