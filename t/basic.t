
my $sp = './speedy';

print "1..2\n";

# Test 1
my $str = "hello_world";
my $line = `$sp t/scripts/basic.1 $str`;
print ($line eq $str ? "ok\n" : "failed\n");

# Test 2
my $num;
system("touch t/scripts/basic.2");
for (my $i = 9; $i > 0; --$i) {
    $num = `$sp t/scripts/basic.2`;
}
print ($num == 9 ? "ok\n" : "failed\n");
