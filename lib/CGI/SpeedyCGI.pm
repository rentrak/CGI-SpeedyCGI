package CGI::SpeedyCGI;

$VERSION = '1.7';

use strict;

sub new { my $class = shift;
    return bless {}, $class;
}

use vars qw($_shutdown_handler);

# Set a handler function to be called before shutting down the Perl process
sub set_shutdown_handler { my($self, $newh) = @_;
    my $oldh = $_shutdown_handler;
    $_shutdown_handler = $newh;
    return $oldh;
}

1;
