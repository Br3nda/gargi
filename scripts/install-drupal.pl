#!/usr/bin/perl
##############################################################################
#
# Script:   drupal_install
#
# Author:   Martyn Smith <martyn@catalyst.net.nz>
#
# Description:
#
# Installs a drupal instance via its web interface
#

use strict;
use warnings;

use Pod::Usage;
use Getopt::Long qw(GetOptions);
use WWW::Mechanize;

my(%opt);

unless ( GetOptions(\%opt, qw(help|? host|h=s profile|p=s verbose|v)) ) {
    pod2usage(-exitval => 1,  -verbose => 0);
}

$opt{url} = shift;


pod2usage(-exitstatus => 0, -verbose => 2) if $opt{help};
pod2usage(-exitstatus => 1, -verbose => 1) unless $opt{url};
if ( $opt{url} =~ m{ install\.php \z }xms ) {
    print "URL ends in 'install.php', it should just be the base path to your drupal\n";
    exit 1;
}

my $profile = $opt{profile} || 'default';

$opt{url} .= '/' unless $opt{url} =~ m{ / \z }xms;

my $mech = WWW::Mechanize->new();

print "Installing at $opt{url}\n" if $opt{verbose};

if ( $opt{host} ) {
    print "Setting Host header to '$opt{host}'\n" if $opt{verbose};
    $mech->proxy('http', undef);
    $mech->add_header('Host' => $opt{host});
}

$mech->get($opt{url} . 'install.php');

unless ( $mech->current_form and $mech->current_form->attr('id') eq 'install-select-profile-form' ) {
    print "Returned form doesn't look like a profile selection form\n";
    if ( $opt{verbose} ) {
        # TODO, be nice to pipe this through w3m if it's available
        print $mech->content();
    }
    exit 1;
}

$mech->submit_form(
    fields => {
        profile => $profile,
    },
);

$mech->follow_link( text_regex => qr{Install Drupal in English} );

# Follow all meta refresh tags
while ( $mech->follow_link('tag' => 'meta') ) {};

my $password = '';
$password .= chr(int(rand()*96)+32) for 1..32;

$mech->submit_form(
    fields => {
        site_name => 'drupal-install',
        site_mail => 'null@catalyst.net.nz',
        'account[name]' => 'root',
        'account[mail]' => 'null@catalyst.net.nz',
        'account[pass][pass1]' => $password,
        'account[pass][pass2]' => $password,
    },
);

exit 0;

__END__

=head1 NAME

drupal_install - Installs a drupal instance via its web interface

=head1 SYNOPSIS

  drupal_install [options] <url>

  Options:

    -?                     detailed help message
    -v, --verbose          verbose output
    -h, --host             use this host header instead of the one in the URL
    -p, --profile=PROFILE  what installation profile to use

=head1 DESCRIPTION

Installs a drupal instance via its web interface

=head1 OPTIONS

=over 4

=item B<-?>

Display this documentation.

=item B<-v>

Verbose output

=item B<-h>

Use the specified host in the Host header for all requests rather than the one
implicitly in the URL. This is useful when DNS isn't working properly yet.

e.g. install-drupal.pl http://localhost -h mysite.example.com

=item B<-p>

Installation profile to use (must be the machine name of the profile)

=item B<url>

The URL to install (should be the base path of the drupal install, this script
will append /install.php to the path)

=back

=cut


