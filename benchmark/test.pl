
use strict;
use CGI;
my $q = CGI->new;
use vars qw($num);

print 'p=', $q->param('p'), "\n";
