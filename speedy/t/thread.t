
my $x = `$ENV{SPEEDY} t/scripts/thread`;
if ($x =~ /no-thread/) {
    print "1..0\n";
} else {
    print "1..1\n";
    my $ok = ($x eq "x\nx\n");
    print $ok ? "ok\n" : "failed\n";
}
