#!/usr/bin/perl

use strict;
use warnings;
use Audio::MPD;
use Time::HiRes qw/usleep/;

my $host = $ARGV[0] || "localhost";

my $mpd = new Audio::MPD( host => $host );

my $show_paused = 0;

# disable output buffering
$| = 1;


my $volume = 0;
my $volume_display = 0;

my $track = 0;
my $track_display = 0;

sub display_volume {
	my ($volume) = @_;
	print "color:255/0/0\n";
	print "lnum:$volume\n";
}

sub display_track {
	my ($track) = @_;
	print "color:0/255/0\n";
	print "num:$track\n";
}

sub display_progress {
	my ($status) = @_;
	my $state = $status->state;
	if ($state eq "play" || ($show_paused && $state eq "pause")) {
		my $song = $status->song;
		my $time = $status->time;
		my $percent = int($time->percent + 0.5);
		my $remaining = $time->seconds_left;
		print "color:0/255/128\n";
		print "lnum:$remaining\n";
	} else {
		print "off\n";
		print "color:0/0/0\n";
	}
}

while (my $status = $mpd->status) {
	# state changes
	my $n_vol = $status->volume;
	if ($n_vol != $volume) {
		$volume = $n_vol;
		$volume_display = 3;
	}
	my $n_track = $status->song;
	if ($n_track != $track) {
		$track = $n_track;
		$track_display = 3;
	}
	if ($volume_display) {
		display_volume($volume);
		$volume_display--;
	} elsif ($track_display) {
		display_track($track);
		$track_display--;
	} else {
		# display progress
		display_progress($status);
	}

	usleep 500000;
}

print "off\n";
print "color:0/0/0\n";
