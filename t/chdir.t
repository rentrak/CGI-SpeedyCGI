
# Bug: if the perl process does a chdir then the next time the process is
# run it is still chdir'ed to that directory.  It should be started in
# the original directory.

my $sp = './speedy';

print "1..1\n";

# Test 1
my $start = `pwd`;
my $spdir;
for (my $i = 0; $i < 2; ++$i) {
    $spdir=`$sp -- -r2 t/scripts/chdir`
}
## print STDERR "Now at: $spdir";
print (($start eq $spdir) ? "ok\n" : "failed\n");
