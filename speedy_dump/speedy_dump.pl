#!/usr/bin/perl

use strict;
require 'SpeedyFile.pl';

my $indent;
my $sw = 4;
sub out {
    foreach my $line (@_) {
	print ' ' x $indent, $line, "\n";
    }
}

sub out_head { my $name = shift;
    &out('', '#', "# $name", '#', @_);
}

sub dump_struct { my($name, $struct) = @_;
    &out_head($name, @{$struct->dump});
}

sub out_struct {
    &out(@{shift->dump});
}

my $f = SpeedyFile->new($ARGV[0]);
my $head = $f->file_head;
my $headval = $head->value;

&dump_struct('File Header', $head);

# Get list of free slots
&out_head('Free Slot List');
if (my $s = $headval->{slot_free}) {
    &out_list($s, 'free_slot');
}

sub out_list { my($cur, $type, $next) = @_;
    $next ||= 'next_slot';
    my $slot = $f->slot($cur, $type);
    while (1) {
	&out_struct($slot);
	if ($cur = $slot->value->{$next}) {
	    $slot = $f->slot($cur, $type);
	} else {
	    last;
	}
	&out('#');
    }
}

&out_head('Group List');
if (my $g = $headval->{group_head}) {
    my $gr = $f->slot($g, 'gr_slot');
    while (1) {
	&out_struct($gr);
	$indent += $sw;
	if (my $n = $gr->value->{name}) {
	    &out_head('Name');
	    my $slot = $f->slot($n, 'grnm_slot');
	    &out_struct($slot);
	}
	&out_head('Script List');
	if (my $s = $gr->value->{script_head}) {
	    &out_list($s, 'scr_slot');
	}
	&out_head('FE Wait List');
	if (my $s = $gr->value->{fe_wait}) {
	    &out_list($s, 'fe_slot');
	}
	&out_head('BE List');
	if (my $s = $gr->value->{be_head}) {
	    &out_list($s, 'be_slot');
	}
	&out_head('BE Wait List');
	if (my $s = $gr->value->{be_wait}) {
	    &out_list($s, 'be_slot', 'be_wait_next');
	}
	$indent -= $sw;
	if (my $n = $gr->value->{next_slot}) {
	    $gr = $f->slot($n, 'gr_slot');
	} else {
	    last;
	}
	&out('');
    }
}
