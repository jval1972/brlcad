package BRLCAD_DOC;

# used for common code for BRL-CAD perl doc utilities
use strict;
use warnings;

use DBPATH;

BEGIN {
  use Exporter;
  our @ISA = qw(Exporter);
  our ($VERSION, @EXPORT, @EXPORT_OK, %EXPORT_TAGS, @EXPORT_FAIL);

  $VERSION = '2011-09-21';

  # Note that exported variables must have their proper leading symbol.
  my @export_ok_all
    = (
       # functions
       'get_svn_status',
       'print_autogen_header',
       'print_color_file',
       'print_insert_color_file_name',
       'print_xhtml_header',
       'print_xml_header',
       'print_book_title',
       'print_brlcad_logo_group',
      );

  my @export_ok
    = (
       # variables
       '$XML_ASCII_HEADER',
       '$XML_UTF8_HEADER',
       '$ascii',
       '$genfopxmlcat',
       '$genxmlcat',
       '$utf8',
      );

  @EXPORT_OK
    = (
       @export_ok_all,
       @export_ok,
      );

  %EXPORT_TAGS
    = (
       'all' => [@export_ok_all],
      );

} # BEGIN

# === global vars for export =============

# output files
our $genxmlcat     = 'brlcad-xml-catalog-autogen.xml';

# special name for the fop file:
our $genfopxmlcat  = 'CatalogManager.properties';

#my $title_border = '4pt double black';
my $title_border = 'none';

my $brlcad_logo_top           = '2.0in'; # inches

my $brlcad_title_top          = '4in';
my $brlcad_title_font         = 'STIXGeneral'; # 'DejaVuLGCSans';
my $brlcad_title_font_size    = '0.4in';

my $brlcad_revision_top       = '8in';
my $brlcad_revision_font      = 'STIXGeneral'; # 'DejaVuLGCSans';
my $brlcad_revision_font_size = '12pt';


our $ascii            = 'ASCII';
our $utf8             = 'UTF-8';
our $XML_ASCII_HEADER = "<?xml version='1.0' encoding='ASCII'?>";
our $XML_UTF8_HEADER  = "<?xml version='1.0' encoding='UTF-8'?>";

# === local vars =========================

# === functions ==========================
sub print_xml_header {
  my $fp     = shift @_;
  my $utf    = shift @_;

  $utf = 0 if !defined $utf;

  if (!$utf) {
    print $fp $XML_ASCII_HEADER, "\n";
  }
  else {
    print $fp $XML_UTF8_HEADER, "\n";
  }

} # print_xml_header

sub print_xhtml_header {
  my $fp     = shift @_;
  my $utf    = shift @_;

  $utf = 0 if !defined $utf;

  if (!$utf) {
    print $fp $XML_ASCII_HEADER, "\n";
  }
  else {
    print $fp $XML_UTF8_HEADER, "\n";
  }

  print $fp <<"HERE";
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">

HERE

} # print_xhtml_header

sub print_autogen_header {
  my $fp     = shift @_;
  my $prog   = shift @_;
  my $xmlhdr = shift @_;
  if (defined $xmlhdr) {
    print_xml_header($fp);
  }
  print $fp <<"HERE";
<!--
 !   This file is autogenerated by '$prog'.
 !   ALL EDITS WILL BE OVERWRITTEN WITHOUT WARNING!
 -->
HERE
} # print_autogen_header

sub print_insert_color_file_name {
  my $fp  = shift @_;
  my $vol = shift @_;

  print $fp <<"HERE";
<xsl:include href="brlcad-colors-autogen-${vol}.xsl"/>
HERE

} # print_insert_color_file_name

sub strip_lines {
  # used to customize and insert data into title pages

  # removes all lines from the beginning until '<?brlcad strip-before ?>'
  # removes all lines '<?brlcad strip-after ?>' to the end

  my $aref = shift @_;
  my @arr = ();
  my $strip = 1;
  #print "debug: starting strip\n";
  foreach my $line (@{$aref}) {
    #print $line;
    if ($line =~ m{\A \s* \<\?brlcad \s+ strip\-before\s* \?\>}xms){
      #print "debug: stopping strip\n";
      $strip = 0;
    }
    elsif (!$strip && $line =~ m{\A \s* \<\?brlcad \s+ strip\-after\s* \?\>}xms) {
      #print "debug: starting strip\n";
      push @arr, $line;
      $strip = 1;
    }
    push @arr, $line if !$strip;
    #print "debug: saving: $line" if !$strip;
  }

  # now replace the old array with the new
  @{$aref} = @arr;

} # strip_lines

sub print_color_file {
  my $fp    = shift @_;
  my $color = shift @_;

  print $fp <<"HERE";
<xsl:stylesheet
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:fo="http://www.w3.org/1999/XSL/Format"
  xmlns:xi="http://www.w3.org/2001/XInclude"
  xmlns:d="http://docbook.org/ns/docbook"
  exclude-result-prefixes="d"
  version="1.0"
>

  <xsl:param name="brlcad.cover.color">$color</xsl:param>

</xsl:stylesheet>
HERE

} # print_color_file

sub print_book_title {
  my $fp       = shift @_;
  my $revision = shift @_;
  my @titles   = @_;

  my $top = $brlcad_title_top;

  print $fp <<"HERE";
      <!-- BRL-CAD BOOK ==================================== -->
      <fo:block-container text-align='center'>
         <fo:block-container
            absolute-position="absolute"
            border='$title_border'
            padding-after='-7pt'
            font-family="$brlcad_title_font"
            font-size="$brlcad_title_font_size"
            font-weight="bold"
            left='1.0in'
            text-align="center"
            top="$top"
            width='6.5in'
          >
HERE

  foreach my $t (@titles) {
    print $fp "           <fo:block>\n";
    print $fp "             $t\n";
    print $fp "           </fo:block>\n";
  }

  print $fp <<"HERE2";
         </fo:block-container>
      </fo:block-container>
HERE2

  # and the revision info
  print $fp <<"HERE3";
      <fo:block-container text-align='center'>
        <fo:block-container
            absolute-position="absolute"
            font-family="$brlcad_title_font"
            font-size="$brlcad_revision_font_size"
            font-weight="normal"
            left='1.0in'
            text-align="center"
            top="$brlcad_revision_top"
            width='6.5in'
        >
          <fo:block>
             subversion revision number: $revision
          </fo:block>
        </fo:block-container>
      </fo:block-container>
HERE3

} # print_book_title

sub print_draft_overlay {
  my $fp  = shift @_;
#  my $top = '6.20in';
#  my $top = '6.00in';
  my $top = '6.75in';

  # convert inches to fo's millipoints
  my $millipoint = 1.0 * 72 * 1000;
  my $transform = "fox:transform='rotate(-45, $millipoint, 0)'";

  #my $color = 'gray';
  my $color = '#C8C8C8';
  print $fp <<HERE;
      <!-- DRAFT OVERLAY ==================================== -->
      <fo:block-container
         $transform
         absolute-position='absolute'
         font-family='LinLib'
         font-size='3.0in'
         font-weight='light'
         text-align='center'
         color='$color'
         top='$top'
      >
        <fo:block>
          DRAFT
        </fo:block>
      </fo:block-container>
HERE

} # print_draft_overlay

sub strip_xml_element {
  my $aref       = shift @_;
  my $element    = shift @_;
  my $element_id = shift @_;

  my @arr = ();
  my $strip   = 0;
  my $find_el = 1;

  my $debug =0;
  print STDERR "debug: starting stripping of element: '$element', id:'$element_id'\n"
    if $debug;
  foreach my $line (@{$aref}) {
    if ($find_el
	&& $line =~ m{\A \s* \<$element \s+}xms
	&& $line =~ m{$element_id}xms
       ) {
      if ($debug) {
	print STDERR "debug: starting strip\n";
	print STDERR $line;
      }
      $strip = 1;
      $find_el = 0;
      next;
    }
    elsif ($strip
	   && $line =~ m{\A \s* \</$element \s* \>}xms
	  ) {
      if ($debug) {
	print STDERR "debug: stopping strip\n";
	print STDERR $line;
      }
      $strip = 0;
      next;
    }
    push @arr, $line
      if !$strip;
    #print "debug: saving: $line" if !$strip;
  }

  # now replace the old array with the new
  @{$aref} = @arr;
} # strip_xml_element


sub strip_fo_bookmark {
  my $aref           = shift @_;
  my $bookmark_title = shift @_;

  my @arr = ();
  my $strip   = 0;
  my $find_el = 1;

  my $debug =0;
  print STDERR "debug: starting stripping of book mark title: '$bookmark_title'\n"
    if $debug;

  my @tarr = @{$aref};
  my $nl = @tarr;

  for (my $i = 0; $i < $nl; ++$i) {
    my $line = $tarr[$i];
    if ($find_el
	&& $line =~ m{\A \s* \<fo:bookmark \s+}xms
       ) {
      # need the next two lines as a set
      my @xlines = ();
      push @xlines, $line;
      my $found_bookmark = 0;
      for (my $j = 0; $j < 2; ++$j) {
	++$i;
	$line = $tarr[$i];
	if ($line =~ m{$bookmark_title}) {
	  $found_bookmark = 1;
	}
	push @xlines, $line;
      }
      my $nx = @xlines;
      die "\@xlines != 3, it's $nx" if (3 != $nx);
      if ($found_bookmark) {
	$find_el = 0;
      }
      else {
	push @arr, @xlines;
      }
      next;
    }

    push @arr, $line;
  }

  # now replace the old array with the new
  @{$aref} = @arr;
} # strip_fo_bookmark

sub get_svn_status {
  my $f = shift @_;
  my @msgs = qx(svn status -v $f);

=pod

  an up-to-date versioned file:
                 46918    46773 tbrowder2    books/en/BRL-CAD_Tutorial_Series-VolumeIII.xml
  a locally-modified versioned file:
    M            46919    46919 tbrowder2    dummy.xml

=cut

  my %codes = ();
  my ($working_rev, $last_commit_rev, $last_commit_author, $path);

  my $has_codes = 0;
  if (@msgs) {
    foreach my $line (@msgs) {
      my @d = split(' ', $line);
      next if !defined $d[0];

      # status codes are in the first 9 columns
      chomp $line;
      for (my $i = 0; $i < 9; ++$i) {
	my $c = substr $line, $i, 1;
        if ($c ne ' ') {
	  $codes{$c} = 1;
	  $has_codes = 1;
	}
      }
      $line = substr $line, 9;
      @d = split(' ', $line);
      $working_rev        = shift @d;
      $last_commit_rev    = shift @d;
      $last_commit_author = shift @d;
      $path = join('', @d);
      if ($path ne $f) {
	print "WARNING:  svn path '$path' not equal file '$f'\n";
      }
      last;
    }
  }
  return ($working_rev, $has_codes);
} # get_svn_status

sub print_brlcad_logo_group {
  my $fp    = shift @_;
  my $color = shift @_;

  # set the original width of the group container
  my $width = '6'; # inches
  # amount to scale it
  my $scale = 0.6;
  # new width
  my $nwidth = $scale * $width;

  # need to convert transform distances to fop's millipoints
  my $dx  = 0.5 * (8.5 - $nwidth); # inches
  $dx    *=   72; # points
  $dx    *= 1000; # millipoints

  # top of group container
  my $top  = $brlcad_logo_top; # inches

  # original: scale(0.5, 0.5)
  print $fp <<"HERE";
      <!-- fox:transform='scale(0.5, 0.5)' -->

      <!-- BRL-CAD LOGO GROUP ============================================================ -->
<fo:block-container
  absolute-position="absolute"
  text-align="center"
  width='6in'
  top='$top'
  fox:transform='translate($dx, 0) scale($scale)'
>

      <!-- note that distances are from the top of the containing block-container -->
      <!-- BRL-CAD LOGO ============================================================ -->
      <!-- originally 2.75in from top absolute -->
      <fo:block-container
         text-align="center">
HERE

  if (1) {
    # svg
    print $fp <<"HERE2a";
        <fo:block>
          <fo:instream-foreign-object content-width='5in' content-height='auto' text-align='center'>
            <xi:include href="$DBPATH::COVER_IMAGES_DIR/brlcad-logo-${color}.svg" parse='xml'>
              <xi:fallback parse="text">
                FIXME:  MISSING XINCLUDE CONTENT
              </xi:fallback>
            </xi:include>
          </fo:instream-foreign-object>
        </fo:block>
HERE2a

  }
  else {
    # png
    print $fp <<"HERE2b";
        <fo:block text-align="center">
          <fo:external-graphic src="url(./brlcad_new_logo_1024x512.svg)" width="100%" height="auto" content-width="scale-to-fit" content-height="scale-to-fit" text-align="center"/>
        </fo:block>
HERE2b
  }

  print $fp <<"HERE3";
      </fo:block-container>

</fo:block-container>
HERE3
} # print_brlcad_logo_group

#======================
1;