
# Test1: test that two scripts with the same group return the same pid
# Test2: same test, but use a long group name the same in the first 12 chars
# Test3: two different group names should return different pids

print "1..3\n";

sub doit { my($grp1, $grp2, $same) = @_;

    utime time, time, 't/scripts/group1';
    utime time, time, 't/scripts/group2';
    sleep 1;
    my $pid1 = `$ENV{SPEEDY} -- -g$grp1 t/scripts/group1`;
    sleep 1;
    my $pid2 = `$ENV{SPEEDY} -- -g$grp2 t/scripts/group2`;

    my $ok = ($pid1 > 0 && $pid2 > 0) &&
	($same ? ($pid1 == $pid2) : ($pid1 != $pid2));

    if ($ok) {
	print "ok\n";
    } else {
	print "failed\n";
    }
}

&doit('', '', 1);
&doit('012345678912xyzzy', '012345678912snarf', 1);
&doit('a', 'b', 0);
