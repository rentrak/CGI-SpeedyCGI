package SpeedyFile;

# c2ph gives us incorrect sizeof's for the total size of the structs

require 'speedy.ph';

sub new { my($class, $fname) = @_;
    bless {fname=>$fname}, $class;
}

sub fname { my $self = shift;
    $self->{fname} ||=
	sprintf('%s.2.%x.F', ($ENV{SPEEDY_TMPBASE} || '/tmp/speedy'), $>);
}

sub data { my $self = shift;
    if (!$self->{data}) {
	open(F, $self->fname) || die $self->fname . ": $!\n";
	my $sz = (stat(F))[7];
	my $data;
	read(F, $data, $sz);
	$self->{data} = $data;
	close(F);
    }
    return $self->{data};
}

sub get_struct { my($self, $type, $offset) = @_;
    if ($type !~ /^_/) {
	$type = '_' . $type;
    }
    SpeedyStruct->new(substr($self->data, $offset, ${"${type}'sizeof"}), $type);
}

sub file_head {
    shift->get_struct('_file_head', 0);
}

my $slot_size = &_grnm_slot'sizeof('name');
my $slots_offset = $_file'offsetof[&_file'slots];

sub slot { my($self, $slotnum, $type) = @_;
    SpeedySlot->new(
	$self->get_struct($type, $slots_offset + ($slotnum-1) * $slot_size),
	$slotnum
    );
}



package SpeedyStruct;

my %pack_template = (
    1=>'C',
    2=>'S',
    4=>'I',
    8=>'Q',
);

sub new { my($class, $data, $type) = @_;
    bless {data=>$data, type=>$type}, $class;
}

sub fieldnames { my $self = shift;
    $self->{fieldnames} ||= [grep {/./} @{$self->{type}. "'fieldnames"}];
}

sub value { my $self = shift;
    if (!$self->{value}) {
	my $type = $self->{type};
	my %value;
	foreach my $field (@{$self->fieldnames}) {
	    my $idx = &{"${type}'$field"};
	    my $size = ${"${type}'sizeof"}[$idx];
	    my $offset = ${"${type}'offsetof"}[$idx];
	    my $value;
	    if ($size == 8) {
		$value = sprintf('0x%x%08x',
		    unpack('I', substr($self->{data}, $offset)),
		    unpack('I', substr($self->{data}, $offset+4))
		);
	    }
	    elsif (my $t = $pack_template{$size}) {
		$value = unpack($t, substr($self->{data}, $offset));
	    }
	    else {
		$value = substr($self->{data}, $offset, $size);
	    }
	    $value{$field} = $value;
	}
	$self->{value} = \%value;
    }
    return $self->{value};
}

sub fmt_key_val { shift;
    sprintf('%-15s = %s', @_);
}

sub dump { my $self = shift;
    my @lines;
    my $value = $self->value;
    foreach my $fld (@{$self->fieldnames}) {
	push(@lines, $self->fmt_key_val($fld, $value->{$fld}));
    }
    push(@lines, $self->fmt_key_val('type', $self->{type}));
    return \@lines;
}



package SpeedySlot;

@ISA = 'SpeedyStruct';

sub new { my($class, $self, $slotnum) = @_;
    $self->{slotnum} = $slotnum;
    bless $self, $class;
}

sub slotnum {shift->{slotnum}}

sub dump { my $self = shift;
    [$self->fmt_key_val('slotnum', $self->slotnum), @{$self->SUPER::dump}];
}

1;
