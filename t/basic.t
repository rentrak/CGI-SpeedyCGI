
my $sp = './speedy';
my $tmpbase = "/tmp/speedy_test.basic";
my $tmpnum;
my $tmp;
my @tmp;
sub mkscript {
    my $tmp = $tmpbase . ++$tmpnum;
    unlink($tmp);
    open(F, ">$tmp") || die("${tmp}: $!");
    print F @_;
    close(F);
    push(@tmp, $tmp);
    return $tmp;
}

print "1..2\n";

# Test 1
my $str = "hello world";
$tmp = mkscript <<END;
print "$str";
END
my $line = `$sp $tmp`;
print ($line eq $str ? "ok\n" : "failed\n");

# Test 2
$tmp = mkscript <<END;
print ++\$x;
END
my $num;
for (my $i = 9; $i > 0; --$i) {
    $num = `$sp $tmp`;
}
print ($num == 9 ? "ok\n" : "failed\n");

grep {unlink($_)} @tmp;
