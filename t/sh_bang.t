
my $sp = './speedy';
my $tmp = '/tmp/sh_bang';

print "1..1\n";

# Test 1
open(F, ">$tmp");
my $here = `pwd`; chomp $here;
print F "#!${here}/speedy -- -r1\nprint \$\$;\n";
close(F);
chmod 0755, $tmp;

my $pid1 = `$tmp`;
my $pid2 = `$tmp`;
print ($pid1 ne $pid2 && $pid1 ne '' && pid2 ne '' ? "ok\n" : "failed\n");
