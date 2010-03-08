#!/usr/bin/perl
###########################################################################
# ABI-compliance-checker v1.13, lightweight tool for statically checking
# backward binary compatibility of shared C/C++ libraries in Linux.
# Copyright (C) The Linux Foundation
# Copyright (C) Institute for System Programming, RAS
# Author: Andrey Ponomarenko
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
###########################################################################
use Getopt::Long;
Getopt::Long::Configure ("posix_default", "no_ignore_case");
use Data::Dumper;

my $ABI_COMPLIANCE_CHECKER_VERSION = "1.13";
my ($Help, $ShowVersion, %Descriptor, $TargetLibraryName, $HeaderCheckingMode_Separately, $GenerateDescriptor, $TestSystem, $DumpInfo_DescriptorPath, $CheckHeadersOnly, $InterfacesListPath, $AppPath, $ShowExpendTime);

my $CmdName = get_FileName($0);
GetOptions("h|help!" => \$Help,
  "v|version!" => \$ShowVersion,
#general options
  "l|library=s" => \$TargetLibraryName,
  "d1|descriptor1=s" => \$Descriptor{1}{"Path"},
  "d2|descriptor2=s" => \$Descriptor{2}{"Path"},
#extra options
  "app|application=s" => \$AppPath,
  "symbols_list|int_list=s" => \$InterfacesListPath,
  "dump_abi|dump_info=s" => \$DumpInfo_DescriptorPath,
  "headers_only!" => \$CheckHeadersOnly,
#other options
  "d|descriptor_template!" => \$GenerateDescriptor,
  "separately!" => \$HeaderCheckingMode_Separately,
  "test!" => \$TestSystem,
  "time!" => \$ShowExpendTime
) or exit(1);

sub HELP_MESSAGE()
{
    print STDERR <<"EOM"

NAME:
  $CmdName - check ABI compatibility of shared C/C++ library versions

DESCRIPTION:
  Lightweight tool for statically checking backward binary compatibility of shared C/C++ libraries
  in Linux. It checks header files along with shared objects in two library versions and searches
  for ABI changes that may lead to incompatibility. Breakage of the binary compatibility may result
  in crashing or incorrect behavior of applications built with an old version of a library when
  it is running with a new one.

  ABI Compliance Checker was intended for library developers that are interested in ensuring
  backward binary compatibility. Also it can be used for checking forward binary compatibility
  and checking applications portability to the new library version.
  
  This tool is free software: you can redistribute it and/or modify it under the terms of the GNU GPL.

USAGE:
  $CmdName [options]

EXAMPLE OF USE:
  $CmdName -l <library_name> -d1 <1st_version_descriptor> -d2 <2nd_version_descriptor>

GENERAL OPTIONS:
  -h|-help
      Print this help.

  -v|-version
      Print version.

  -l|-library <name>
      Library name (without version).
      It affects only on the path and the title of the report.

  -d1|-descriptor1 <path>
      Path to descriptor of 1st library version.

  -d2|-descriptor2 <path>
      Path to descriptor of 2nd library version.

EXTRA OPTIONS:
  -app|-application <path>
      This option allow to specify the application that should be tested for portability
      to the new library version.

  -dump_abi|-dump_info <descriptor_path>
      Dump library ABI information using specified descriptor.
      This command will create '<library>_<ver1>.abi.tar.gz' file in the directory 'abi_dumps/<library>/'.
      You can transfer it anywhere and pass instead of library descriptor.

  -headers_only
      Check header files without shared objects. It is easy to run, but may provide
      a low quality ABI compliance report with false positives and without
      detecting of added/withdrawn interfaces.

  -symbols_list|-int_list <path>
      This option allow to specify a file with a list of interfaces (mangled names in C++)
      that should be checked, other library interfaces will not be checked.

OTHER OPTIONS:
  -d|-descriptor_template
      Create library descriptor template 'library-descriptor.xml' in the current directory.

  -separately
      Check headers individually. This mode requires more time for checking ABI compliance,
      but possible compiler errors in one header can't affect others.

  -test
      Run internal tests (create two binary-incompatible versions of an artificial library
      and run ABI-Compliance-Checker on it).

DESCRIPTOR EXAMPLE:
  <version>
     1.28.0
  </version>

  <headers>
     /usr/local/atk/atk-1.28.0/include/
  </headers>

  <libs>
     /usr/local/atk/atk-1.28.0/lib/libatk-1.0.so
  </libs>

  <include_paths>
     /usr/include/glib-2.0/
     /usr/lib/glib-2.0/include/
  </include_paths>


Report bugs to <abi-compliance-checker\@linuxtesting.org>
For more information, please see: http://ispras.linux-foundation.org/index.php/ABI_compliance_checker
EOM
      ;
}

my $Descriptor_Template = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<descriptor>

<!-- Template for the library version descriptor -->

<!--
     Necessary sections  
                        -->

<version>
    <!-- Version of the library -->
</version>

<headers>
    <!-- The list of paths to header files and/or
         directories with header files, one per line -->
</headers>

<libs>
    <!-- The list of paths to shared objects and/or
         directories with shared objects, one per line -->
</libs>

<!--
     Additional sections
                         -->

<include_paths>
    <!-- The list of paths to be searched for header files
         needed for compiling of library headers, one per line -->
</include_paths>

<gcc_options>
    <!-- Additional gcc options, one per line -->
</gcc_options>

<opaque_types>
    <!-- The list of opaque types, one per line -->
</opaque_types>

<skip_interfaces>
    <!-- The list of functions (mangled/symbol names in C++)
         that should be skipped while testing, one per line -->
</skip_interfaces>

<include_preamble>
    <!-- The list of header files that should be included before other headers, one per line.
         For example, it is a tree.h for libxml2 and ft2build.h for freetype2 -->
</include_preamble>

</descriptor>";

my %Operator_Indication = (
"not" => "~",
"assign" => "=",
"andassign" => "&=",
"orassign" => "|=",
"xorassign" => "^=",
"or" => "|",
"xor" => "^",
"addr" => "&",
"and" => "&",
"lnot" => "!",
"eq" => "==",
"ne" => "!=",
"lt" => "<",
"lshift" => "<<",
"lshiftassign" => "<<=",
"rshiftassign" => ">>=",
"call" => "()",
"mod" => "%",
"modassign" => "%=",
"subs" => "[]",
"land" => "&&",
"lor" => "||",
"rshift" => ">>",
"ref" => "->",
"le" => "<=",
"deref" => "*",
"mult" => "*",
"preinc" => "++",
"delete" => " delete",
"vecnew" => " new[]",
"vecdelete" => " delete[]",
"predec" => "--",
"postinc" => "++",
"postdec" => "--",
"plusassign" => "+=",
"plus" => "+",
"minus" => "-",
"minusassign" => "-=",
"gt" => ">",
"ge" => ">=",
"new" => " new",
"multassign" => "*=",
"divassign" => "/=",
"div" => "/",
"neg" => "-",
"pos" => "+",
"memref" => "->*",
"compound" => ","
);

sub num_to_str($)
{
    my $Number = $_[0];
    if(int($Number)>3)
    {
        return $Number."th";
    }
    elsif(int($Number)==1)
    {
        return "1st";
    }
    elsif(int($Number)==2)
    {
        return "2nd";
    }
    elsif(int($Number)==3)
    {
        return "3rd";
    }
    else
    {
        return "";
    }
}

#Global variables
my $REPORT_PATH;
my %ERR_PATH;
my $POINTER_SIZE;
my $MAX_COMMAND_LINE_ARGUMENTS = 4096;
my %Cache;
my %FuncAttr;
my %LibInfo;
my %HeaderCompileError;
my $StartTime;
my %CompilerOptions;
my %AddedInt;
my %WithdrawnInt;
my @RecurLib;
my %CheckedSoLib;

#Constants checking
my %ConstantsSrc;
my %Constants;

#Types
my %TypeDescr;
my %TemplateInstance_Func;
my %TemplateInstance;
my %OpaqueTypes;
my %Tid_TDid;
my %CheckedTypes;
my %Typedef_BaseName;
my %StdCxxTypedef;
my %TName_Tid;
my %EnumMembName_Id;

#Interfaces
my %FuncDescr;
my %ClassFunc;
my %ClassVirtFunc;
my %ClassIdVirtFunc;
my %ClassId;
my %tr_name;
my %mangled_name;
my %InternalInterfaces;
my %InterfacesList;
my %InterfacesList_App;
my %CheckedInterfaces;
my %DepInterfaces;

#Headers
my %Include_Preamble;
my %Headers;
my %HeaderName_Destinations;
my %Header_Dependency;

#Shared objects
my %SoLib_DefaultPath;

#Merging
my %CompleteSignature;
my @RecurTypes;
my %Interface_Library;
my %Library_Interface;
my %Language;
my %SoNames_All;
my $Version;

#Symbols versioning
my %SymVer;

#Problem descriptions
my %CompatProblems;
my %ConstantProblems;

#Rerorts
my $ContentID = 1;
my $ContentSpanStart = "<span class=\"section\" onclick=\"javascript:showContent(this, 'CONTENT_ID')\">\n";
my $ContentSpanEnd = "</span>\n";
my $ContentDivStart = "<div id=\"CONTENT_ID\" style=\"display:none;\">\n";
my $ContentDivEnd = "</div>\n";
my $Content_Counter = 0;

sub readDescriptor($)
{
    my $LibVersion = $_[0];
    if(not -e $Descriptor{$LibVersion}{"Path"})
    {
        return;
    }
    my $Descriptor_File = readFile($Descriptor{$LibVersion}{"Path"});
    $Descriptor_File =~ s/\/\*(.|\n)+?\*\///g;
    $Descriptor_File =~ s/<\!--(.|\n)+?-->//g;
    if(not $Descriptor_File)
    {
        print "ERROR: descriptor d$LibVersion is empty\n";
        exit(1);
    }
    $Descriptor{$LibVersion}{"Version"} = parseTag(\$Descriptor_File, "version");
    if(not $Descriptor{$LibVersion}{"Version"})
    {
        print "ERROR: version in the descriptor d$LibVersion was not specified (section <version>)\n\n";
        exit(1);
    }
    $Descriptor{$LibVersion}{"Headers"} = parseTag(\$Descriptor_File, "headers");
    if(not $Descriptor{$LibVersion}{"Headers"})
    {
        print "ERROR: header files in the descriptor d$LibVersion were not specified (section <headers>)\n";
        exit(1);
    }
    if(not $CheckHeadersOnly)
    {
        $Descriptor{$LibVersion}{"Libs"} = parseTag(\$Descriptor_File, "libs");
        if(not $Descriptor{$LibVersion}{"Libs"})
        {
            print "ERROR: shared objects in the descriptor d$LibVersion were not specified (section <libs>)\n";
            exit(1);
        }
    }
    $Descriptor{$LibVersion}{"Include_Paths"} = parseTag(\$Descriptor_File, "include_paths");
    $Descriptor{$LibVersion}{"Gcc_Options"} = parseTag(\$Descriptor_File, "gcc_options");
    foreach my $Option (split("\n", $Descriptor{$LibVersion}{"Gcc_Options"}))
    {
        $Option =~ s/\A\s+|\s+\Z//g;
        next if(not $Option);
        $CompilerOptions{$LibVersion} .= " ".$Option;
    }
    $Descriptor{$LibVersion}{"Opaque_Types"} = parseTag(\$Descriptor_File, "opaque_types");
    foreach my $Type_Name (split("\n", $Descriptor{$LibVersion}{"Opaque_Types"}))
    {
        $Type_Name =~ s/\A\s+|\s+\Z//g;
        next if(not $Type_Name);
        $OpaqueTypes{$LibVersion}{$Type_Name} = 1;
    }
    $Descriptor{$LibVersion}{"Skip_interfaces"} = parseTag(\$Descriptor_File, "skip_interfaces");
    foreach my $Interface_Name (split("\n", $Descriptor{$LibVersion}{"Skip_interfaces"}))
    {
        $Interface_Name =~ s/\A\s+|\s+\Z//g;
        next if(not $Interface_Name);
        $InternalInterfaces{$LibVersion}{$Interface_Name} = 1;
    }
    $Descriptor{$LibVersion}{"Include_Preamble"} = parseTag(\$Descriptor_File, "include_preamble");
    my $Position = 0;
    foreach my $Header_Name (split("\n", $Descriptor{$LibVersion}{"Include_Preamble"}))
    {
        $Header_Name =~ s/\A\s+|\s+\Z//g;
        next if(not $Header_Name);
        $Include_Preamble{$LibVersion}{$Header_Name}{"Position"} = $Position;
        $Position+=1;
    }
    my $Descriptors_Dir = "descriptors_storage/$TargetLibraryName";
    system("mkdir", "-p", $Descriptors_Dir);
    my $Descriptor_Name = $TargetLibraryName."_".$Descriptor{$LibVersion}{"Version"}.".desc";
    if($Descriptor{$LibVersion}{"Path"} ne $Descriptors_Dir."/".$Descriptor_Name)
    {
        system("cp", "-f", $Descriptor{$LibVersion}{"Path"}, $Descriptors_Dir."/".$Descriptor_Name);
    }
    $ERR_PATH{$LibVersion} = "header_compile_errors/$TargetLibraryName/".$Descriptor{$LibVersion}{"Version"};
}

sub parseTag($$)
{
    my ($CodeRef, $Tag) = @_;
    return "" if(not $CodeRef or not ${$CodeRef} or not $Tag);
    if(${$CodeRef} =~ s/\<$Tag\>((.|\n)+?)\<\/$Tag\>//)
    {
        my $Content = $1;
        $Content=~s/(\A\s+|\s+\Z)//g;
        return $Content;
    }
    else
    {
        return "";
    }
}

my %check_node=(
"array_type"=>1,
"binfo"=>1,
"boolean_type"=>1,
"complex_type"=>1,
"const_decl"=>1,
"enumeral_type"=>1,
"field_decl"=>1,
"function_decl"=>1,
"function_type"=>1,
"identifier_node"=>1,
"integer_cst"=>1,
"integer_type"=>1,
"method_type"=>1,
"namespace_decl"=>1,
"parm_decl"=>1,
"pointer_type"=>1,
"real_cst"=>1,
"real_type"=>1,
"record_type"=>1,
"reference_type"=>1,
"string_cst"=>1,
"template_decl"=>1,
"template_type_parm"=>1,
"tree_list"=>1,
"tree_vec"=>1,
"type_decl"=>1,
"union_type"=>1,
"var_decl"=>1,
"void_type"=>1);

sub getInfo($)
{
	my $InfoPath = $_[0];
    return if(not $InfoPath or not -f $InfoPath);
    my $InfoPath_New = $InfoPath.".1";
    #my $Keywords = join("\\|", keys(%check_node));#|grep "$Keywords"
    system("sed ':a;N;\$!ba;s/\\n[^\@]//g' ".esc($InfoPath)."|sed 's/ [ ]\\+/  /g' > ".esc($InfoPath_New));
    system("rm", "-fr", $InfoPath);
    #getting info
    open(INFO, $InfoPath_New) || die ("can't open file '\$InfoPath_New\': $!\n");
    while(<INFO>)
    {
        chomp;
        if(/\A@([0-9]+)[ ]+([a-zA-Z_]+)[ ]+(.*)\Z/)
        {
            next if(not $check_node{$2});
            $LibInfo{$Version}{$1}{"info_type"}=$2;
            $LibInfo{$Version}{$1}{"info"}=$3;
        }
    }
    close(INFO);
    system("rm", "-fr", $InfoPath_New);
    #processing info
	setTemplateParams_All();
	getTypeDescr_All();
	getFuncDescr_All();
	getVarDescr_All();
    %LibInfo = ();
    %TemplateInstance = ();
}

sub setTemplateParams_All()
{
    foreach (keys(%{$LibInfo{$Version}}))
    {
        if($LibInfo{$Version}{$_}{"info_type"} eq "template_decl")
        {
            setTemplateParams($_);
        }
    }
}

sub setTemplateParams($)
{
    my $TypeInfoId = $_[0];
    my $Info = $LibInfo{$Version}{$TypeInfoId}{"info"};
    if($Info =~ /(inst|spcs)[ ]*:[ ]*@([0-9]+) /)
    {
        my $TmplInst_InfoId = $2;
        setTemplateInstParams($TmplInst_InfoId);
        my $TmplInst_Info = $LibInfo{$Version}{$TmplInst_InfoId}{"info"};
        while($TmplInst_Info =~ /chan[ ]*:[ ]*@([0-9]+) /)
        {
            $TmplInst_InfoId = $1;
            $TmplInst_Info = $LibInfo{$Version}{$TmplInst_InfoId}{"info"};
            setTemplateInstParams($TmplInst_InfoId);
        }
    }
}

sub setTemplateInstParams($)
{
    my $TmplInst_Id = $_[0];
    my $Info = $LibInfo{$Version}{$TmplInst_Id}{"info"};
    my ($Params_InfoId, $ElemId) = ();
    if($Info =~ /purp[ ]*:[ ]*@([0-9]+) /)
    {
        $Params_InfoId = $1;
    }
    if($Info =~ /valu[ ]*:[ ]*@([0-9]+) /)
    {
        $ElemId = $1;
    }
    if($Params_InfoId and $ElemId)
    {
        my $Params_Info = $LibInfo{$Version}{$Params_InfoId}{"info"};
        while($Params_Info =~ s/ ([0-9]+)[ ]*:[ ]*@([0-9]+) //)
        {
            my ($Param_Pos, $Param_TypeId) = ($1, $2);
            return if($LibInfo{$Version}{$Param_TypeId}{"info_type"} eq "template_type_parm");
            if($LibInfo{$ElemId}{"info_type"} eq "function_decl")
            {
                $TemplateInstance_Func{$Version}{$ElemId}{$Param_Pos} = $Param_TypeId;
            }
            else
            {
                $TemplateInstance{$Version}{getTypeDeclId($ElemId)}{$ElemId}{$Param_Pos} = $Param_TypeId;
            }
        }
    }
}

sub getTypeDeclId($)
{
	my $TypeInfo = $LibInfo{$Version}{$_[0]}{"info"};
	if($TypeInfo =~ /name[ ]*:[ ]*@([0-9]+)/)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub isFuncPtr($)
{
    my $Ptd = pointTo($_[0]);
	if($Ptd)
	{
		if(($LibInfo{$Version}{$_[0]}{"info"} =~ m/unql[ ]*:/) and not ($LibInfo{$Version}{$_[0]}{"info"} =~ m/qual[ ]*:/))
		{
			return 0;
		}
		elsif(($LibInfo{$Version}{$_[0]}{"info_type"} eq "pointer_type") and ($LibInfo{$Version}{$Ptd}{"info_type"} eq "function_type" or $LibInfo{$Version}{$Ptd}{"info_type"} eq "method_type"))
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		return 0;
	}
}

sub pointTo($)
{
	my $TypeInfo = $LibInfo{$Version}{$_[0]}{"info"};
	if($TypeInfo =~ /ptd[ ]*:[ ]*@([0-9]+)/)
    {
	    return $1;
    }
    else
    {
        return "";
    }
}

sub getTypeDescr_All()
{
    foreach (sort {int($a)<=>int($b)} keys(%{$LibInfo{$Version}}))
    {
        if($LibInfo{$Version}{$_}{"info_type"}=~/_type\Z/ and $LibInfo{$Version}{$_}{"info_type"}!~/function_type|method_type/)
        {
            getTypeDescr(getTypeDeclId($_), $_);
        }
    }
    $TypeDescr{$Version}{""}{-1}{"Name"} = "...";
    $TypeDescr{$Version}{""}{-1}{"Type"} = "Intrinsic";
    $TypeDescr{$Version}{""}{-1}{"Tid"} = -1;
}

sub getTypeDescr($$)
{
    my ($TypeDeclId, $TypeId) = @_;
    $Tid_TDid{$Version}{$TypeId} = $TypeDeclId;
    %{$TypeDescr{$Version}{$TypeDeclId}{$TypeId}} = getTypeAttr($TypeDeclId, $TypeId);
    if(not $TypeDescr{$Version}{$TypeDeclId}{$TypeId}{"Name"})
    {
        delete($TypeDescr{$Version}{$TypeDeclId}{$TypeId});
        return;
    }
    if(not $TName_Tid{$Version}{$TypeDescr{$Version}{$TypeDeclId}{$TypeId}{"Name"}})
    {
        $TName_Tid{$Version}{$TypeDescr{$Version}{$TypeDeclId}{$TypeId}{"Name"}} = $TypeId;
    }
}

sub getTypeAttr($$)
{
    my ($TypeDeclId, $TypeId) = @_;
    my ($BaseTypeSpec, %TypeAttr) = ();
    if($TypeDescr{$Version}{$TypeDeclId}{$TypeId}{"Name"})
    {
        return %{$TypeDescr{$Version}{$TypeDeclId}{$TypeId}};
    }
    $TypeAttr{"Tid"} = $TypeId;
    $TypeAttr{"TDid"} = $TypeDeclId;
    $TypeAttr{"Type"} = getTypeType($TypeDeclId, $TypeId);
    if($TypeAttr{"Type"} eq "Unknown")
    {
        return ();
    }
    elsif($TypeAttr{"Type"} eq "FuncPtr")
    {
        %{$TypeDescr{$Version}{$TypeDeclId}{$TypeId}} = getFuncPtrAttr(pointTo($TypeId), $TypeDeclId, $TypeId);
        $TName_Tid{$Version}{$TypeDescr{$Version}{$TypeDeclId}{$TypeId}{"Name"}} = $TypeId;
        return %{$TypeDescr{$Version}{$TypeDeclId}{$TypeId}};
    }
    elsif($TypeAttr{"Type"} eq "Array")
    {
        ($TypeAttr{"BaseType"}{"Tid"}, $TypeAttr{"BaseType"}{"TDid"}, $BaseTypeSpec) = selectBaseType($TypeDeclId, $TypeId);
        my %BaseTypeAttr = getTypeAttr($TypeAttr{"BaseType"}{"TDid"}, $TypeAttr{"BaseType"}{"Tid"});
        my $ArrayElemNum = getSize($TypeId)/8;
        $ArrayElemNum = $ArrayElemNum/$BaseTypeAttr{"Size"} if($BaseTypeAttr{"Size"});
        $TypeAttr{"Size"} = $ArrayElemNum;
        if($ArrayElemNum)
        {
            $TypeAttr{"Name"} = $BaseTypeAttr{"Name"}."[".$ArrayElemNum."]";
        }
        else
        {
            $TypeAttr{"Name"} = $BaseTypeAttr{"Name"}."[]";
        }
        $TypeAttr{"Name"} = correctName($TypeAttr{"Name"});
        $TypeAttr{"Library"} = $BaseTypeAttr{"Library"};
        $TypeAttr{"Header"} = $BaseTypeAttr{"Header"};
        %{$TypeDescr{$Version}{$TypeDeclId}{$TypeId}} = %TypeAttr;
        $TName_Tid{$Version}{$TypeDescr{$Version}{$TypeDeclId}{$TypeId}{"Name"}} = $TypeId;
        return %TypeAttr;
    }
    elsif($TypeAttr{"Type"} =~ /Intrinsic|Union|Struct|Enum|Class/)
    {
        if($TemplateInstance{$Version}{$TypeDeclId}{$TypeId})
        {
            my @Template_Params = ();
            foreach my $Param_Pos (sort {int($a)<=>int($b)} keys(%{$TemplateInstance{$Version}{$TypeDeclId}{$TypeId}}))
            {
                my $Type_Id = $TemplateInstance{$Version}{$TypeDeclId}{$TypeId}{$Param_Pos};
                my $Param = get_TemplateParam($Type_Id);
                if($Param eq "")
                {
                    return ();
                }
                @Template_Params = (@Template_Params, $Param);
            }
            %{$TypeDescr{$Version}{$TypeDeclId}{$TypeId}} = getTrivialTypeAttr($TypeDeclId, $TypeId);
            $TypeDescr{$Version}{$TypeDeclId}{$TypeId}{"Name"} = $TypeDescr{$Version}{$TypeDeclId}{$TypeId}{"Name"}."< ".join(", ", @Template_Params)." >";
            $TypeDescr{$Version}{$TypeDeclId}{$TypeId}{"Name"} = correctName($TypeDescr{$Version}{$TypeDeclId}{$TypeId}{"Name"});
            $TName_Tid{$Version}{$TypeDescr{$Version}{$TypeDeclId}{$TypeId}{"Name"}} = $TypeId;
            return %{$TypeDescr{$Version}{$TypeDeclId}{$TypeId}};
        }
        else
        {
            %{$TypeDescr{$Version}{$TypeDeclId}{$TypeId}} = getTrivialTypeAttr($TypeDeclId, $TypeId);
            return %{$TypeDescr{$Version}{$TypeDeclId}{$TypeId}};
        }
    }
    else
    {
        ($TypeAttr{"BaseType"}{"Tid"}, $TypeAttr{"BaseType"}{"TDid"}, $BaseTypeSpec) = selectBaseType($TypeDeclId, $TypeId);
        my %BaseTypeAttr = getTypeAttr($TypeAttr{"BaseType"}{"TDid"}, $TypeAttr{"BaseType"}{"Tid"});
        if($BaseTypeSpec and $BaseTypeAttr{"Name"})
        {
            if(($TypeAttr{"Type"} eq "Pointer") and $BaseTypeAttr{"Name"}=~/\([\*]+\)/)
            {
                $TypeAttr{"Name"} = $BaseTypeAttr{"Name"};
                $TypeAttr{"Name"} =~ s/\(([*]+)\)/($1*)/g;
            }
            else
            {
                $TypeAttr{"Name"} = $BaseTypeAttr{"Name"}." ".$BaseTypeSpec;
            }
        }
        elsif($BaseTypeAttr{"Name"})
        {
            $TypeAttr{"Name"} = $BaseTypeAttr{"Name"};
        }
        if($TypeAttr{"Type"} eq "Typedef")
        {
            $TypeAttr{"Name"} = getNameByInfo($TypeDeclId);
            $TypeAttr{"NameSpace"} = getNameSpace($TypeDeclId);
            if($TypeAttr{"NameSpace"})
            {
                $TypeAttr{"Name"} = $TypeAttr{"NameSpace"}."::".$TypeAttr{"Name"};
            }
            ($TypeAttr{"Header"}, $TypeAttr{"Line"}) = getLocation($TypeDeclId);
            if($TypeAttr{"NameSpace"}=~/\Astd(::|\Z)/ and $BaseTypeAttr{"NameSpace"}=~/\Astd(::|\Z)/)
            {
                $StdCxxTypedef{$Version}{$BaseTypeAttr{"Name"}} = $TypeAttr{"Name"};
            }
            $Typedef_BaseName{$Version}{$TypeAttr{"Name"}} = $BaseTypeAttr{"Name"};
        }
        if(not $TypeAttr{"Size"})
        {
            if($TypeAttr{"Type"} eq "Pointer")
            {
                $TypeAttr{"Size"} = $POINTER_SIZE;
            }
            else
            {
                $TypeAttr{"Size"} = $BaseTypeAttr{"Size"};
            }
        }
        $TypeAttr{"Name"} = correctName($TypeAttr{"Name"});
        $TypeAttr{"Library"} = $BaseTypeAttr{"Library"};
        $TypeAttr{"Header"} = $BaseTypeAttr{"Header"} if(not $TypeAttr{"Header"});
        %{$TypeDescr{$Version}{$TypeDeclId}{$TypeId}} = %TypeAttr;
        $TName_Tid{$Version}{$TypeDescr{$Version}{$TypeDeclId}{$TypeId}{"Name"}} = $TypeId;
        return %TypeAttr;
    }
}

sub get_TemplateParam($)
{
    my $Type_Id = $_[0];
    if(getNodeType($Type_Id) eq "integer_cst")
    {
        return getNodeIntCst($Type_Id);
    }
    elsif(getNodeType($Type_Id) eq "string_cst")
    {
        return getNodeStrCst($Type_Id);
    }
    else
    {
        my $Type_DId = getTypeDeclId($Type_Id);
        my %ParamAttr = getTypeAttr($Type_DId, $Type_Id);
        if(not $ParamAttr{"Name"})
        {
            return "";
        }
        if($ParamAttr{"Name"}=~/\>/)
        {
            if($StdCxxTypedef{$Version}{$ParamAttr{"Name"}})
            {
                return $StdCxxTypedef{$Version}{$ParamAttr{"Name"}};
            }
            elsif(my $Covered = cover_stdcxx_typedef($ParamAttr{"Name"}))
            {
                return $Covered;
            }
            else
            {
                return $ParamAttr{"Name"};
            }
        }
        else
        {
            return $ParamAttr{"Name"};
        }
    }
}

sub cover_stdcxx_typedef($)
{
    my $TypeName = $_[0];
    my $TypeName_Covered = $TypeName;
    while($TypeName=~s/>[ ]*(const|volatile|restrict| |\*|\&)\Z/>/g){};
    if(my $Cover = $StdCxxTypedef{$Version}{$TypeName})
    {
        $TypeName = esc_l($TypeName);
        $TypeName_Covered=~s/$TypeName/$Cover /g;
    }
    return correctName($TypeName_Covered);
}

sub getNodeType($)
{
    return $LibInfo{$Version}{$_[0]}{"info_type"};
}

sub getNodeIntCst($)
{
    my $CstId = $_[0];
    my $CstTypeId = getTreeAttr($CstId, "type");
    if($EnumMembName_Id{$Version}{$CstId})
    {
        return $EnumMembName_Id{$Version}{$CstId};
    }
    elsif($LibInfo{$Version}{$_[0]}{"info"} =~ /low[ ]*:[ ]*([^ ]+) /)
    {
        if($1 eq "0")
        {
            if(getNodeType($CstTypeId) eq "boolean_type")
            {
                return "false";
            }
            else
            {
                return "0";
            }
        }
        elsif($1 eq "1")
        {
            if(getNodeType($CstTypeId) eq "boolean_type")
            {
                return "true";
            }
            else
            {
                return "1";
            }
        }
        else
        {
            return $1;
        }
    }
    else
    {
        return "";
    }
}

sub getNodeStrCst($)
{
    if($LibInfo{$Version}{$_[0]}{"info"} =~ /low[ ]*:[ ]*(.+)[ ]+lngt/)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub esc_l($)
{
    my $String = $_[0];
    $String=~s/([()*])/\\$1/g;
    return $String;
}

sub getFuncPtrAttr($$$)
{
    my ($FuncTypeId, $TypeDeclId, $TypeId) = @_;
    my $FuncInfo = $LibInfo{$Version}{$FuncTypeId}{"info"};
    my $FuncInfo_Type = $LibInfo{$Version}{$FuncTypeId}{"info_type"};
    my $FuncPtrCorrectName = "";
    my %TypeAttr = ("Size"=>$POINTER_SIZE, "Type"=>"FuncPtr", "TDid"=>$TypeDeclId, "Tid"=>$TypeId);
    my @ParamTypeName;
    if($FuncInfo =~ /retn[ ]*:[ ]*\@([0-9]+) /)
    {
        my $ReturnTypeId = $1;
        my %ReturnAttr = getTypeAttr(getTypeDeclId($ReturnTypeId), $ReturnTypeId);
        $FuncPtrCorrectName .= $ReturnAttr{"Name"};
        $TypeAttr{"Return"} = $ReturnTypeId;
    }
    if($FuncInfo =~ /prms[ ]*:[ ]*@([0-9]+) /)
    {
        my $ParamTypeInfoId = $1;
        my $Position = 0;
        while($ParamTypeInfoId)
        {
            my $ParamTypeInfo = $LibInfo{$Version}{$ParamTypeInfoId}{"info"};
            last if($ParamTypeInfo !~ /valu[ ]*:[ ]*@([0-9]+) /);
            my $ParamTypeId = $1;
            my %ParamAttr = getTypeAttr(getTypeDeclId($ParamTypeId), $ParamTypeId);
            last if($ParamAttr{"Name"} eq "void");
            $TypeAttr{"Memb"}{$Position}{"type"} = $ParamTypeId;
            push(@ParamTypeName, $ParamAttr{"Name"});
            last if($ParamTypeInfo !~ /chan[ ]*:[ ]*@([0-9]+) /);
            $ParamTypeInfoId = $1;
            $Position+=1;
        }
    }
    if($FuncInfo_Type eq "function_type")
    {
        $FuncPtrCorrectName .= " (*) (".join(", ", @ParamTypeName).")";
    }
    elsif($FuncInfo_Type eq "method_type")
    {
        if($FuncInfo =~ /clas[ ]*:[ ]*@([0-9]+) /)
        {
            my $ClassId = $1;
            my $ClassName = $TypeDescr{$Version}{getTypeDeclId($ClassId)}{$ClassId}{"Name"};
            if($ClassName)
            {
                $FuncPtrCorrectName .= " ($ClassName\:\:*) (".join(", ", @ParamTypeName).")";
            }
            else
            {
                $FuncPtrCorrectName .= " (*) (".join(", ", @ParamTypeName).")";
            }
        }
        else
        {
            $FuncPtrCorrectName .= " (*) (".join(", ", @ParamTypeName).")";
        }
    }
    $TypeAttr{"Name"} = correctName($FuncPtrCorrectName);
    return %TypeAttr;
}

sub getTypeName($)
{
	my $Info = $LibInfo{$Version}{$_[0]}{"info"};
	if($Info =~ /name[ ]*:[ ]*@([0-9]+) /)
	{
		return getNameByInfo($1);
	}
	else
	{
		if($LibInfo{$Version}{$_[0]}{"info_type"} eq "integer_type")
		{
			if($LibInfo{$Version}{$_[0]}{"info"} =~ /unsigned/)
			{
				return "unsigned int";
			}
			else
			{
				return "int";
			}
		}
		else
		{
			return "";
		}
	}
}

sub selectBaseType($$)
{
	my ($TypeDeclId, $TypeId) = @_;
	my $TypeInfo = $LibInfo{$Version}{$TypeId}{"info"};
	my $BaseTypeDeclId;
    my $Type_Type = getTypeType($TypeDeclId, $TypeId);
	#qualifications
	if(($LibInfo{$Version}{$TypeId}{"info"} =~ /qual[ ]*:[ ]*c /) and ($LibInfo{$Version}{$TypeId}{"info"} =~ /unql[ ]*:[ ]*\@([0-9]+) /))
	{
		return ($1, getTypeDeclId($1), "const");
	}
	elsif(($LibInfo{$Version}{$TypeId}{"info"} =~ /qual[ ]*:[ ]*r /) and ($LibInfo{$Version}{$TypeId}{"info"} =~ /unql[ ]*:[ ]*\@([0-9]+) /))
	{
		return ($1, getTypeDeclId($1), "restrict");
	}
	elsif(($LibInfo{$Version}{$TypeId}{"info"} =~ /qual[ ]*:[ ]*v /) and ($LibInfo{$Version}{$TypeId}{"info"} =~ /unql[ ]*:[ ]*\@([0-9]+) /))
	{
		return ($1, getTypeDeclId($1), "volatile");
	}
	elsif((not ($LibInfo{$Version}{$TypeId}{"info"} =~ /qual[ ]*:/)) and ($LibInfo{$Version}{$TypeId}{"info"} =~ /unql[ ]*:[ ]*\@([0-9]+) /))
	{#typedefs
		return ($1, getTypeDeclId($1), "");
	}
	elsif($LibInfo{$Version}{$TypeId}{"info_type"} eq "reference_type")
	{
		if($TypeInfo =~ /refd[ ]*:[ ]*@([0-9]+) /)
        {
		    return ($1, getTypeDeclId($1), "&");
        }
        else
        {
            return (0, 0, "");
        }
	}
	elsif($LibInfo{$Version}{$TypeId}{"info_type"} eq "array_type")
	{
		if($TypeInfo =~ /elts[ ]*:[ ]*@([0-9]+) /)
        {
		    return ($1, getTypeDeclId($1), "");
        }
        else
        {
            return (0, 0, "");
        }
	}
	elsif($LibInfo{$Version}{$TypeId}{"info_type"} eq "pointer_type")
	{
		if($TypeInfo =~ /ptd[ ]*:[ ]*@([0-9]+) /)
        {
		    return ($1, getTypeDeclId($1), "*");
        }
        else
        {
            return (0, 0, "");
        }
	}
	else
	{
		return (0, 0, "");
	}
}

sub getFuncDescr_All()
{
	foreach (sort {int($b)<=>int($a)} keys(%{$LibInfo{$Version}}))
	{
		if($LibInfo{$Version}{$_}{"info_type"} eq "function_decl")
		{
			getFuncDescr($_);
		}
	}
}

sub getVarDescr_All()
{
	foreach (sort {int($b)<=>int($a)} keys(%{$LibInfo{$Version}}))
	{
		if($LibInfo{$Version}{$_}{"info_type"} eq "var_decl")
		{
			getVarDescr($_);
		}
	}
}

sub getVarDescr($)
{
	my $FuncInfoId = $_[0];
    if($LibInfo{$Version}{getNameSpaceId($FuncInfoId)}{"info_type"} eq "function_decl")
    {
        return;
    }
    ($FuncDescr{$Version}{$FuncInfoId}{"Header"}, $FuncDescr{$Version}{$FuncInfoId}{"Line"}) = getLocation($FuncInfoId);
    if((not $FuncDescr{$Version}{$FuncInfoId}{"Header"}) or ($FuncDescr{$Version}{$FuncInfoId}{"Header"}=~/\<built\-in\>|\<internal\>/))
    {
        delete($FuncDescr{$Version}{$FuncInfoId});
        return;
    }
    $FuncDescr{$Version}{$FuncInfoId}{"ShortName"} = getNameByInfo($FuncInfoId);
    $FuncDescr{$Version}{$FuncInfoId}{"MnglName"} = getFuncMnglName($FuncInfoId);
    if($FuncDescr{$Version}{$FuncInfoId}{"MnglName"} and $FuncDescr{$Version}{$FuncInfoId}{"MnglName"}!~/\A_Z/)
    {
        delete($FuncDescr{$Version}{$FuncInfoId});
        return;
    }
    if(not $FuncDescr{$Version}{$FuncInfoId}{"MnglName"})
    {
        $FuncDescr{$Version}{$FuncInfoId}{"Name"} = $FuncDescr{$Version}{$FuncInfoId}{"ShortName"};
        $FuncDescr{$Version}{$FuncInfoId}{"MnglName"} = $FuncDescr{$Version}{$FuncInfoId}{"ShortName"};
    }
    if(not is_in_library($FuncDescr{$Version}{$FuncInfoId}{"MnglName"}, $Version) and not $CheckHeadersOnly)
    {
        delete $FuncDescr{$Version}{$FuncInfoId};
        return;
    }
	$FuncDescr{$Version}{$FuncInfoId}{"Return"} = getTypeId($FuncInfoId);
    delete($FuncDescr{$Version}{$FuncInfoId}{"Return"}) if(not $FuncDescr{$Version}{$FuncInfoId}{"Return"});
	$FuncDescr{$Version}{$FuncInfoId}{"Data"} = 1;
	set_Class_And_Namespace($FuncInfoId);
	setFuncAccess($FuncInfoId);
	if($FuncDescr{$Version}{$FuncInfoId}{"MnglName"} =~ /\A_ZTV/)
    {
        delete($FuncDescr{$Version}{$FuncInfoId}{"Return"});
    }
    if($FuncDescr{$Version}{$FuncInfoId}{"ShortName"} =~ /\A_Z/)
    {
        delete($FuncDescr{$Version}{$FuncInfoId}{"ShortName"});
    }
}

sub getTrivialTypeAttr($$)
{
	my ($TypeInfoId, $TypeId) = @_;
    my %TypeAttr = ();
    return if(getTypeTypeByTypeId($TypeId)!~/Intrinsic|Union|Struct|Enum/);
	setTypeAccess($TypeId, \%TypeAttr);
	($TypeAttr{"Header"}, $TypeAttr{"Line"}) = getLocation($TypeInfoId);
	if(($TypeAttr{"Header"} eq "<built-in>") or ($TypeAttr{"Header"} eq "<internal>"))
	{
        delete($TypeAttr{"Header"});
	}
    $TypeAttr{"Name"} = getNameByInfo($TypeInfoId);
    $TypeAttr{"Name"} = getTypeName($TypeId) if(not $TypeAttr{"Name"});
    my $NameSpaceId = getNameSpaceId($TypeInfoId);
    if($NameSpaceId ne $TypeId)
    {
        $TypeAttr{"NameSpace"} = getNameSpace($TypeInfoId);
    }
    if($TypeAttr{"NameSpace"} and isNotAnon($TypeAttr{"Name"}))
    {
        $TypeAttr{"Name"} = $TypeAttr{"NameSpace"}."::".$TypeAttr{"Name"};
    }
    $TypeAttr{"Name"} = correctName($TypeAttr{"Name"});
	if(isAnon($TypeAttr{"Name"}))
	{
		$TypeAttr{"Name"} = "anon-";
		$TypeAttr{"Name"} .= $TypeAttr{"Header"}."-".$TypeAttr{"Line"};
	}
	$TypeAttr{"Size"} = getSize($TypeId)/8;
	$TypeAttr{"Type"} = getTypeType($TypeInfoId, $TypeId);
    if($TypeAttr{"Type"} eq "Struct" and has_methods($TypeId))
    {
        $TypeAttr{"Type"} = "Class";
    }
    if(($TypeAttr{"Type"} eq "Struct") or ($TypeAttr{"Type"} eq "Class"))
    {
        setBaseClasses($TypeInfoId, $TypeId, \%TypeAttr);
    }
	setTypeMemb($TypeInfoId, $TypeId, \%TypeAttr);
    $TypeAttr{"Tid"} = $TypeId;
    $TypeAttr{"TDid"} = $TypeInfoId;
    $Tid_TDid{$Version}{$TypeId} = $TypeInfoId;
    if(not $TName_Tid{$Version}{$TypeAttr{"Name"}})
    {
        $TName_Tid{$Version}{$TypeAttr{"Name"}} = $TypeId;
    }
    return %TypeAttr;
}

sub has_methods($)
{
    my $TypeId = $_[0];
    my $Info = $LibInfo{$Version}{$TypeId}{"info"};
    return ($Info=~/(fncs)[ ]*:[ ]*@([0-9]+) /);
}

sub setBaseClasses($$$)
{
    my ($TypeInfoId, $TypeId, $TypeAttr) = @_;
    my $Info = $LibInfo{$Version}{$TypeId}{"info"};
    if($Info =~ /binf[ ]*:[ ]*@([0-9]+) /)
    {
        $Info = $LibInfo{$Version}{$1}{"info"};
        while($Info =~ /accs[ ]*:[ ]*([a-z]+) /)
        {
            last if($Info !~ s/accs[ ]*:[ ]*([a-z]+) //);
            my $Access = $1;
            last if($Info !~ s/binf[ ]*:[ ]*@([0-9]+) //);
            my $BInfoId = $1;
            my $ClassId = getBinfClassId($BInfoId);
            if($Access eq "pub")
            {
                $TypeAttr->{"BaseClass"}{$ClassId} = "public";
            }
            elsif($Access eq "prot")
            {
                $TypeAttr->{"BaseClass"}{$ClassId} = "protected";
            }
            elsif($Access eq "priv")
            {
                $TypeAttr->{"BaseClass"}{$ClassId} = "private";
            }
            else
            {
                $TypeAttr->{"BaseClass"}{$ClassId} = "private";
            }
        }
    }
}

sub getBinfClassId($)
{
    my $Info = $LibInfo{$Version}{$_[0]}{"info"};
    $Info =~ /type[ ]*:[ ]*@([0-9]+) /;
    return $1;
}

sub get_func_signature($)
{
    my $FuncInfoId = $_[0];
    my $PureSignature = $FuncDescr{$Version}{$FuncInfoId}{"ShortName"};
    my @ParamTypes = ();
    foreach my $ParamPos (sort {int($a) <=> int($b)} keys(%{$FuncDescr{$Version}{$FuncInfoId}{"Param"}}))
    {#checking parameters
        my $ParamType_Id = $FuncDescr{$Version}{$FuncInfoId}{"Param"}{$ParamPos}{"type"};
        my $ParamType_Name = uncover_typedefs($TypeDescr{$Version}{getTypeDeclId($ParamType_Id)}{$ParamType_Id}{"Name"});
        @ParamTypes = (@ParamTypes, $ParamType_Name);
    }
    $PureSignature = $PureSignature."(".join(", ", @ParamTypes).")";
    $PureSignature = delete_keywords($PureSignature);
    return correctName($PureSignature);
}

sub delete_keywords($)
{
    my $TypeName = $_[0];
    $TypeName =~ s/(\W|\A)(enum |struct |union |class )/$1/g;
    return $TypeName;
}

sub uncover_typedefs($)
{
    my $TypeName = $_[0];
    return "" if(not $TypeName);
    return $Cache{"uncover_typedefs"}{$Version}{$TypeName} if(defined $Cache{"uncover_typedefs"}{$Version}{$TypeName});
    my ($TypeName_New, $TypeName_Pre) = (correctName($TypeName), "");
    while($TypeName_New ne $TypeName_Pre)
    {
        $TypeName_Pre = $TypeName_New;
        my $TypeName_Copy = $TypeName_New;
        my %Words = ();
        while($TypeName_Copy=~s/(\W|\A)([a-z_][\w:]*)(\W|\Z)//io)
        {
            my $Word = $2;
            next if(not $Word or $Word=~/\A(true|false|const|int|long|void|short|float|unsigned|char|double|class|struct|union|enum)\Z/);
            $Words{$Word} = 1;
        }
        foreach my $Word (keys(%Words))
        {
            my $BaseType_Name = $Typedef_BaseName{$Version}{$Word};
            next if($TypeName_New=~/(\W|\A)(struct $Word|union $Word|enum $Word)(\W|\Z)/);
            next if(not $BaseType_Name);
            if($BaseType_Name=~/\([*]+\)/)
            {
                $TypeName_New =~ /$Word(.*)\Z/;
                my $Type_Suffix = $1;
                $TypeName_New = $BaseType_Name;
                if($TypeName_New =~ s/\(([*]+)\)/($1 $Type_Suffix)/)
                {
                    $TypeName_New = correctName($TypeName_New);
                }
            }
            else
            {
                if($TypeName_New =~ s/(\W|\A)$Word(\W|\Z)/$1$BaseType_Name$2/g)
                {
                    $TypeName_New = correctName($TypeName_New);
                }
            }
        }
    }
    $Cache{"uncover_typedefs"}{$Version}{$TypeName} = $TypeName_New;
    return $TypeName_New;
}

sub isInternal($)
{
    my $FuncInfoId = $_[0];
    my $FuncInfo = $LibInfo{$Version}{$_[0]}{"info"};
    return 0 if($FuncInfo !~ /mngl[ ]*:[ ]*@([0-9]+) /);
    my $FuncMnglNameInfoId = $1;
    return ($LibInfo{$Version}{$FuncMnglNameInfoId}{"info"} =~ /\*[ ]*INTERNAL[ ]*\*/);
}

sub set_Class_And_Namespace($)
{
    my $FuncInfoId = $_[0];
    my $FuncInfo = $LibInfo{$Version}{$FuncInfoId}{"info"};
    if($FuncInfo =~ /scpe[ ]*:[ ]*@([0-9]+) /)
    {
        my $NameSpaceInfoId = $1;
        if($LibInfo{$Version}{$NameSpaceInfoId}{"info_type"} eq "namespace_decl")
        {
            my $NameSpaceInfo = $LibInfo{$Version}{$NameSpaceInfoId}{"info"};
            if($NameSpaceInfo =~ /name[ ]*:[ ]*@([0-9]+) /)
            {
                my $NameSpaceId = $1;
                my $NameSpaceIdentifier = $LibInfo{$Version}{$NameSpaceId}{"info"};
                if($NameSpaceIdentifier =~ /strg[ ]*:[ ]*(.*)[ ]+lngt/)
                {
                    my $NameSpace = $1;
                    $NameSpace =~ s/[ ]+\Z//g;
                    $FuncDescr{$Version}{$FuncInfoId}{"NameSpace"} = $NameSpace;
                }
            }
        }
        elsif($LibInfo{$Version}{$NameSpaceInfoId}{"info_type"} eq "record_type")
        {
            $FuncDescr{$Version}{$FuncInfoId}{"Class"} = $NameSpaceInfoId;
        }
    }
}

sub getFuncDescr($)
{
	my $FuncInfoId = $_[0];
    return if(isInternal($FuncInfoId));
    ($FuncDescr{$Version}{$FuncInfoId}{"Header"}, $FuncDescr{$Version}{$FuncInfoId}{"Line"}) = getLocation($FuncInfoId);
    if(not $FuncDescr{$Version}{$FuncInfoId}{"Header"} or $FuncDescr{$Version}{$FuncInfoId}{"Header"}=~/\<built\-in\>|\<internal\>/)
    {
        delete($FuncDescr{$Version}{$FuncInfoId});
        return;
    }
    setFuncAccess($FuncInfoId);
	setFuncKind($FuncInfoId);
	if($FuncDescr{$Version}{$FuncInfoId}{"PseudoTemplate"})
    {
        delete($FuncDescr{$Version}{$FuncInfoId});
        return;
    }
    $FuncDescr{$Version}{$FuncInfoId}{"Type"} = getFuncType($FuncInfoId);
    $FuncDescr{$Version}{$FuncInfoId}{"Return"} = getFuncReturn($FuncInfoId);
    delete($FuncDescr{$Version}{$FuncInfoId}{"Return"}) if(not $FuncDescr{$Version}{$FuncInfoId}{"Return"});
    $FuncDescr{$Version}{$FuncInfoId}{"ShortName"} = getFuncShortName(getFuncOrig($FuncInfoId));
    if($FuncDescr{$Version}{$FuncInfoId}{"ShortName"} =~ /\._/)
    {
        delete($FuncDescr{$Version}{$FuncInfoId});
        return;
    }
    if(defined $TemplateInstance_Func{$Version}{$FuncInfoId})
    {
        my @TmplParams = ();
        foreach my $ParamPos (sort {int($a) <=> int($b)} keys(%{$TemplateInstance_Func{$Version}{$FuncInfoId}}))
        {
            my $Param = get_TemplateParam($TemplateInstance_Func{$Version}{$FuncInfoId}{$ParamPos});
            if($Param eq "")
            {
                delete($FuncDescr{$Version}{$FuncInfoId});
                return;
            }
            push(@TmplParams, $Param);
        }
        $FuncDescr{$Version}{$FuncInfoId}{"ShortName"} .= "<".join(", ", @TmplParams).">";
    }
    setFuncParams($FuncInfoId);
    $FuncDescr{$Version}{$FuncInfoId}{"MnglName"} = getFuncMnglName($FuncInfoId);
    if($FuncDescr{$Version}{$FuncInfoId}{"MnglName"} and $FuncDescr{$Version}{$FuncInfoId}{"MnglName"}!~/\A_Z/)
    {
        delete($FuncDescr{$Version}{$FuncInfoId});
        return;
    }
    
    if((is_in_library($FuncDescr{$Version}{$FuncInfoId}{"ShortName"}, $Version) or $CheckHeadersOnly) and not $FuncDescr{$Version}{$FuncInfoId}{"MnglName"} and ($FuncDescr{$Version}{$FuncInfoId}{"Type"} eq "Function"))
    {
        $FuncDescr{$Version}{$FuncInfoId}{"MnglName"} = $FuncDescr{$Version}{$FuncInfoId}{"ShortName"};
    }
    set_Class_And_Namespace($FuncInfoId);
    if(not $FuncDescr{$Version}{$FuncInfoId}{"MnglName"} and not $FuncDescr{$Version}{$FuncInfoId}{"Class"})
    {#this section only for c++ functions without class that have not been mangled in the tree
        $FuncDescr{$Version}{$FuncInfoId}{"MnglName"} = $mangled_name{get_func_signature($FuncInfoId)};
    }
    if(not is_in_library($FuncDescr{$Version}{$FuncInfoId}{"MnglName"}, $Version) and not $CheckHeadersOnly)
    {#src only
        delete($FuncDescr{$Version}{$FuncInfoId});
        return;
    }
    if($FuncDescr{$Version}{$FuncInfoId}{"Constructor"} or $FuncDescr{$Version}{$FuncInfoId}{"Destructor"})
    {
        delete($FuncDescr{$Version}{$FuncInfoId}{"Return"});
    }
    my $FuncBody = getFuncBody($FuncInfoId);
    if($FuncBody eq "defined")
    {
        $FuncDescr{$Version}{$FuncInfoId}{"InLine"} = 1;
    }
    if($CheckHeadersOnly and $FuncDescr{$Version}{$FuncInfoId}{"InLine"})
    {
        delete($FuncDescr{$Version}{$FuncInfoId});
        return;
    }
    if(($FuncDescr{$Version}{$FuncInfoId}{"Type"} eq "Method") or $FuncDescr{$Version}{$FuncInfoId}{"Constructor"} or $FuncDescr{$Version}{$FuncInfoId}{"Destructor"})
    {
        if($FuncDescr{$Version}{$FuncInfoId}{"MnglName"}!~/\A_Z/)
        {
            delete($FuncDescr{$Version}{$FuncInfoId});
            return;
        }
    }
	if(getFuncSpec($FuncInfoId) eq "Virt")
	{#virtual methods
		$FuncDescr{$Version}{$FuncInfoId}{"Virt"} = 1;
	}
	if(getFuncSpec($FuncInfoId) eq "PureVirt")
	{#pure virtual methods
		$FuncDescr{$Version}{$FuncInfoId}{"PureVirt"} = 1;
	}
    if($FuncDescr{$Version}{$FuncInfoId}{"MnglName"} =~ /\A_Z/ and $FuncDescr{$Version}{$FuncInfoId}{"Class"})
    {
	    if($FuncDescr{$Version}{$FuncInfoId}{"Type"} eq "Function")
	    {#static methods
		    $FuncDescr{$Version}{$FuncInfoId}{"Static"} = 1;
	    }
    }
	if(getFuncLink($FuncInfoId) eq "Static")
    {
        $FuncDescr{$Version}{$FuncInfoId}{"Static"} = 1;
    }
    delete($FuncDescr{$Version}{$FuncInfoId}{"Type"});
}

sub getFuncBody($)
{
    my $FuncInfo = $LibInfo{$Version}{$_[0]}{"info"};
    if($FuncInfo =~ /body[ ]*:[ ]*undefined(\ |\Z)/i)
    {
        return "undefined";
    }
    elsif($FuncInfo =~ /body[ ]*:[ ]*@([0-9]+)(\ |\Z)/i)
    {
        return "defined";
    }
    else
    {
        return "";
    }
}

sub getTypeShortName($)
{
	my $TypeName = $_[0];
	$TypeName =~ s/\<.*\>//g;
	$TypeName =~ s/.*\:\://g;
	return $TypeName;
}

sub getBackRef($)
{
	my $TypeInfo = $LibInfo{$Version}{$_[0]}{"info"};
	if($TypeInfo =~ /name[ ]*:[ ]*@([0-9]+) /)
    {
	    return $1;
    }
    else
    {
        return "";
    }
}

sub getTypeId($)
{
	my $TypeInfo = $LibInfo{$Version}{$_[0]}{"info"};
	if($TypeInfo =~ /type[ ]*:[ ]*@([0-9]+) /)
    {
	    return $1;
    }
    else
    {
        return "";
    }
}

sub getFuncId($)
{
	my $FuncInfo = $LibInfo{$Version}{$_[0]}{"info"};
	if($FuncInfo =~ /type[ ]*:[ ]*@([0-9]+) /)
    {
	    return $1;
    }
    else
    {
        return "";
    }
}

sub setTypeMemb($$)
{
    my ($TypeDeclId, $TypeId, $TypeAttr) = @_;
	my $TypeInfo = $LibInfo{$Version}{$TypeId}{"info"};
	my $TypeMembInfoId;
	my $TypeType = $TypeAttr->{"Type"};
	my $Position = 0;
    my $BasePosition = 0;
	my $TypeTypeInfoId;
	my $StructMembName;
	if($TypeType eq "Enum")
	{
		$TypeMembInfoId = getEnumMembInfoId($TypeId);
		while($TypeMembInfoId)
		{
			$TypeAttr->{"Memb"}{$Position}{"value"} = getEnumMembVal($TypeMembInfoId);
            my $MembName = getEnumMembName($TypeMembInfoId);
			$TypeAttr->{"Memb"}{$Position}{"name"} = getEnumMembName($TypeMembInfoId);
            $EnumMembName_Id{$Version}{getTreeAttr($TypeMembInfoId, "valu")} = ($TypeAttr->{"NameSpace"})?$TypeAttr->{"NameSpace"}."::".$MembName:$MembName;
			$TypeMembInfoId = getNextMembInfoId($TypeMembInfoId);
			$Position += 1;
		}
	}
	elsif(($TypeType eq "Struct") or ($TypeType eq "Class") or ($TypeType eq "Union"))
	{
		$TypeMembInfoId = getStructMembInfoId($TypeId);
		while($TypeMembInfoId)
		{
            if($LibInfo{$Version}{$TypeMembInfoId}{"info_type"} ne "field_decl")
            {
                $TypeMembInfoId = getNextStructMembInfoId($TypeMembInfoId);
                next;
            }
            $StructMembName = getStructMembName($TypeMembInfoId);
            if($StructMembName =~ /_vptr\./)
            {#virtual tables
                $TypeMembInfoId = getNextStructMembInfoId($TypeMembInfoId);
                next;
            }
            if(not $StructMembName)
            {#base classes
                #$TypeAttr->{"Base"}{$BasePosition}{"type"} = getStructMembType($TypeMembInfoId);
                #$TypeAttr->{"Base"}{$BasePosition}{"access"} = getStructMembAccess($TypeMembInfoId);
                $BasePosition += 1;
                $TypeMembInfoId = getNextStructMembInfoId($TypeMembInfoId);
                next;
            }
            $TypeAttr->{"Memb"}{$Position}{"type"} = getStructMembType($TypeMembInfoId);
            $TypeAttr->{"Memb"}{$Position}{"name"} = $StructMembName;
            $TypeAttr->{"Memb"}{$Position}{"access"} = getStructMembAccess($TypeMembInfoId);
            $TypeAttr->{"Memb"}{$Position}{"bitfield"} = getStructMembBitFieldSize($TypeMembInfoId);
            
			$TypeMembInfoId = getNextStructMembInfoId($TypeMembInfoId);
			$Position += 1;
		}
	}
}

sub setFuncParams($)
{
	my $FuncInfoId = $_[0];
	my $ParamInfoId = getFuncParamInfoId($FuncInfoId);
	my $FunctionType = getFuncType($FuncInfoId);
	if($FunctionType eq "Method")
	{
		$ParamInfoId = getNextElem($ParamInfoId);
	}
    my $Position = 0;
	while($ParamInfoId)
	{
		my $ParamTypeId = getFuncParamType($ParamInfoId);
		last if($TypeDescr{$Version}{getTypeDeclId($ParamTypeId)}{$ParamTypeId}{"Name"} eq "void");
		if($TypeDescr{$Version}{getTypeDeclId($ParamTypeId)}{$ParamTypeId}{"Type"} eq "Restrict")
		{#delete restrict spec
			$ParamTypeId = getRestrictBase($ParamTypeId);
		}
		$FuncDescr{$Version}{$FuncInfoId}{"Param"}{$Position}{"type"} = $ParamTypeId;
		$FuncDescr{$Version}{$FuncInfoId}{"Param"}{$Position}{"name"} = getFuncParamName($ParamInfoId);
        if(not $FuncDescr{$Version}{$FuncInfoId}{"Param"}{$Position}{"name"})
        {
            $FuncDescr{$Version}{$FuncInfoId}{"Param"}{$Position}{"name"} = "p".($Position+1);
        }
		$ParamInfoId = getNextElem($ParamInfoId);
		$Position += 1;
	}
    if(detect_nolimit_args($FuncInfoId))
    {
        $FuncDescr{$Version}{$FuncInfoId}{"Param"}{$Position}{"type"} = -1;
    }
}

sub detect_nolimit_args($)
{
    my $FuncInfoId = $_[0];
    my $FuncTypeId = getFuncTypeId($FuncInfoId);
    my $ParamListElemId = getFuncParamTreeListId($FuncTypeId);
    my $HaveVoid = 0;
    my $Position = 0;
    while($ParamListElemId)
    {
        my $ParamTypeId = getTreeAttr($ParamListElemId, "valu");
        if($TypeDescr{$Version}{getTypeDeclId($ParamTypeId)}{$ParamTypeId}{"Name"} eq "void")
        {
            $HaveVoid = 1;
            last;
        }
        $ParamListElemId = getNextElem($ParamListElemId);
        $Position += 1;
    }
    return ($Position>=1 and not $HaveVoid);
}

sub getFuncParamTreeListId($)
{
    my $FuncTypeId = $_[0];
    if($LibInfo{$Version}{$FuncTypeId}{"info"} =~ /prms[ ]*:[ ]*@([0-9]+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getTreeAttr($$)
{
    my ($Id, $Attr) = @_;
    if($LibInfo{$Version}{$Id}{"info"} =~ /$Attr[ ]*:[ ]*@([0-9]+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getRestrictBase($)
{
	my $TypeId = $_[0];
	my $TypeDeclId = getTypeDeclId($TypeId);
	my $BaseTypeId = $TypeDescr{$Version}{$TypeDeclId}{$TypeId}{"BaseType"}{"Tid"};
	my $BaseTypeDeclId = $TypeDescr{$Version}{$TypeDeclId}{$TypeId}{"BaseType"}{"TDid"};
	return $BaseTypeId;
}

sub setFuncAccess($)
{
	my $FuncInfoId = $_[0];
    if($LibInfo{$Version}{$FuncInfoId}{"info"} =~ /accs[ ]*:[ ]*([a-zA-Z]+) /)
    {
        my $Access = $1;
        if($Access eq "prot")
        {
            $FuncDescr{$Version}{$FuncInfoId}{"Protected"} = 1;
        }
        elsif($Access eq "priv")
        {
            $FuncDescr{$Version}{$FuncInfoId}{"Private"} = 1;
        }
    }
}

sub setTypeAccess($$)
{
    my ($TypeId, $TypeAttr) = @_;
	my $TypeInfo = $LibInfo{$Version}{$TypeId}{"info"};
	if($TypeInfo =~ /accs[ ]*:[ ]*([a-zA-Z]+) /)
    {
	    my $Access = $1;
	    if($Access eq "prot")
	    {
		    $TypeAttr->{"Protected"} = 1;
	    }
	    elsif($Access eq "priv")
	    {
		    $TypeAttr->{"Private"} = 1;
	    }
    }
}

sub setFuncKind($)
{
	my $FuncInfoId = $_[0];
    if($LibInfo{$Version}{$FuncInfoId}{"info"} =~ /pseudo tmpl/)
    {
        $FuncDescr{$Version}{$FuncInfoId}{"PseudoTemplate"} = 1;
    }
    elsif($LibInfo{$Version}{$FuncInfoId}{"info"} =~ /note[ ]*:[ ]*constructor /)
    {
        $FuncDescr{$Version}{$FuncInfoId}{"Constructor"} = 1;
    }
    elsif($LibInfo{$Version}{$FuncInfoId}{"info"} =~ /note[ ]*:[ ]*destructor /)
    {
        $FuncDescr{$Version}{$FuncInfoId}{"Destructor"} = 1;
    }
}

sub getFuncSpec($)
{
	my $FuncInfoId = $_[0];
	my $FuncInfo = $LibInfo{$Version}{$FuncInfoId}{"info"};
	if($FuncInfo =~ m/spec[ ]*:[ ]*pure /)
	{
		return "PureVirt";
	}
	elsif($FuncInfo =~ m/spec[ ]*:[ ]*virt /)
	{
		return "Virt";
	}
	else
	{
		if($FuncInfo =~ /spec[ ]*:[ ]*([a-zA-Z]+) /)
        {
		    return $1;
        }
        else
        {
            return "";
        }
	}
}

sub getFuncClass($)
{
	my $FuncInfoId = $_[0];
	my $FuncInfo = $LibInfo{$Version}{$FuncInfoId}{"info"};
	if($FuncInfo =~ /scpe[ ]*:[ ]*@([0-9]+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getFuncLink($)
{
	my $FuncInfoId = $_[0];
	my $FuncInfo = $LibInfo{$Version}{$FuncInfoId}{"info"};
	if($FuncInfo =~ /link[ ]*:[ ]*static /)
	{
		return "Static";
	}
	else
	{
		if($FuncInfo =~ /link[ ]*:[ ]*([a-zA-Z]+) /)
        {
            return $1;
        }
        else
        {
            return "";
        }
	}
}

sub getNextElem($)
{
	my $FuncInfoId = $_[0];
	my $FuncInfo = $LibInfo{$Version}{$FuncInfoId}{"info"};
	if($FuncInfo =~ /chan[ ]*:[ ]*@([0-9]+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getFuncParamInfoId($)
{
	my $FuncInfoId = $_[0];
	my $FuncInfo = $LibInfo{$Version}{$FuncInfoId}{"info"};
	if($FuncInfo =~ /args[ ]*:[ ]*@([0-9]+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getFuncParamType($)
{
	my $ParamInfoId = $_[0];
	my $ParamInfo = $LibInfo{$Version}{$ParamInfoId}{"info"};
	if($ParamInfo =~ /type[ ]*:[ ]*@([0-9]+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getFuncParamName($)
{
	my $ParamInfoId = $_[0];
	my $ParamInfo = $LibInfo{$Version}{$ParamInfoId}{"info"};
	return "" if($ParamInfo !~ /name[ ]*:[ ]*@([0-9]+) /);
	my $NameInfoId = $1;
	return "" if($LibInfo{$Version}{$NameInfoId}{"info"} !~ /strg[ ]*:[ ]*(.*)[ ]+lngt/);
	my $FuncParamName = $1;
	$FuncParamName =~ s/[ ]+\Z//g;
	return $FuncParamName;
}

sub getEnumMembInfoId($)
{
	my $TypeInfoId = $_[0];
	my $TypeInfo = $LibInfo{$Version}{$TypeInfoId}{"info"};
	if($TypeInfo =~ /csts[ ]*:[ ]*@([0-9]+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getStructMembInfoId($)
{
	my $TypeInfoId = $_[0];
	my $TypeInfo = $LibInfo{$Version}{$TypeInfoId}{"info"};
	if($TypeInfo =~ /flds[ ]*:[ ]*@([0-9]+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getNameSpace($)
{
    my $TypeInfoId = $_[0];
    my $TypeInfo = $LibInfo{$Version}{$TypeInfoId}{"info"};
    return "" if($TypeInfo !~ /scpe[ ]*:[ ]*@([0-9]+) /);
    my $NameSpaceInfoId = $1;
    if($LibInfo{$Version}{$NameSpaceInfoId}{"info_type"} eq "namespace_decl")
    {
        my $NameSpaceInfo = $LibInfo{$Version}{$NameSpaceInfoId}{"info"};
        if($NameSpaceInfo =~ /name[ ]*:[ ]*@([0-9]+) /)
        {
            my $NameSpaceId = $1;
            my $NameSpaceIdentifier = $LibInfo{$Version}{$NameSpaceId}{"info"};
            return "" if($NameSpaceIdentifier !~ /strg[ ]*:[ ]*(.*)[ ]+lngt/);
            my $NameSpace = $1;
            $NameSpace =~ s/[ ]+\Z//g;
            my $BaseNameSpace = getNameSpace($NameSpaceInfoId);
            $NameSpace = $BaseNameSpace."::".$NameSpace if($BaseNameSpace);
            return $NameSpace;
        }
        else
        {
            return "";
        }
    }
    elsif($LibInfo{$Version}{$NameSpaceInfoId}{"info_type"} eq "record_type")
    {
        my %NameSpaceAttr = getTypeAttr(getTypeDeclId($NameSpaceInfoId), $NameSpaceInfoId);
        return $NameSpaceAttr{"Name"};
    }
    else
    {
        return "";
    }
}

sub getNameSpaceId($)
{
	my $TypeInfoId = $_[0];
	my $TypeInfo = $LibInfo{$Version}{$TypeInfoId}{"info"};
	if($TypeInfo =~ /scpe[ ]*:[ ]*@([0-9]+) /)
    {
	    return $1;
    }
    else
    {
        return "";
    }
}

sub getEnumMembName($)
{
	my $TypeMembInfoId = $_[0];
	return "" if($LibInfo{$Version}{$TypeMembInfoId}{"info"} !~ /purp[ ]*:[ ]*@([0-9]+)/);
	my $Purp = $1;
	return "" if($LibInfo{$Version}{$Purp}{"info"} !~ /strg[ ]*:[ ]*(.*)[ ]+lngt/);
	my $EnumMembName = $1;
	$EnumMembName =~ s/[ ]+\Z//g;
	return $EnumMembName;
}

sub getStructMembName($)
{
	my $TypeMembInfoId = $_[0];
	return "" if($LibInfo{$Version}{$TypeMembInfoId}{"info"} !~ /name[ ]*:[ ]*@([0-9]+) /);
	my $NameInfoId = $1;
	return "" if($LibInfo{$Version}{$NameInfoId}{"info"} !~ /strg[ ]*:[ ]*(.*)[ ]+lngt/);
	my $StructMembName = $1;
	$StructMembName =~ s/[ ]+\Z//g;
	return $StructMembName;
}

sub getEnumMembVal($)
{
	my $TypeMembInfoId = $_[0];
	return "" if($LibInfo{$Version}{$TypeMembInfoId}{"info"} !~ /valu[ ]*:[ ]*@([0-9]+) /);
	my $Valu = $1;
	if($LibInfo{$Version}{$Valu}{"info"} =~ /low[ ]*:[ ]*(-?[0-9]+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getSize($)
{
	my $Info = $LibInfo{$Version}{$_[0]}{"info"};
	if($Info =~ /size[ ]*:[ ]*@([0-9]+) /)
	{
        my $SizeInfoId = $1;
		if($LibInfo{$Version}{$SizeInfoId}{"info"} =~ /low[ ]*:[ ]*(-?[0-9]+) /)
        {
		    return $1;
        }
        else
        {
            return "";
        }
	}
	else
	{
		return 0;
	}
}

sub getStructMembType($)
{
	my $TypeMembInfoId = $_[0];
	if($LibInfo{$Version}{$TypeMembInfoId}{"info"} =~ /type[ ]*:[ ]*@([0-9]+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getStructMembBitFieldSize($)
{
	my $TypeMembInfoId = $_[0];
	if($LibInfo{$Version}{$TypeMembInfoId}{"info"} =~ / bitfield /)
	{
		return getSize($TypeMembInfoId);
	}
	else
	{
		return 0;
	}
}

sub getStructMembAccess($)
{
	my $MembInfo = $LibInfo{$Version}{$_[0]}{"info"};
	if($MembInfo =~ /accs[ ]*:[ ]*([a-zA-Z]+) /)
    {
	    my $Access = $1;
	    if($Access eq "prot")
	    {
		    return "protected";
	    }
	    elsif($Access eq "priv")
	    {
		    return "private";
	    }
	    else
	    {
		    return "public";
	    }
    }
    else
    {
        return "public";
    }
}

sub getNextMembInfoId($)
{
	my $TypeMembInfoId = $_[0];
	if($LibInfo{$Version}{$TypeMembInfoId}{"info"} =~ /chan[ ]*:[ ]*@([0-9]+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getNextStructMembInfoId($)
{
	my $TypeMembInfoId = $_[0];
	if($LibInfo{$Version}{$TypeMembInfoId}{"info"} =~ /chan[ ]*:[ ]*@([0-9]+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub fieldHasName($)
{
	my $TypeMembInfoId = $_[0];
	if($LibInfo{$Version}{$TypeMembInfoId}{"info_type"} eq "field_decl")
	{
		if($LibInfo{$Version}{$TypeMembInfoId}{"info"} =~ /name[ ]*:[ ]*@([0-9]+) /)
        {
            return $1;
        }
        else
        {
            return "";
        }
	}
	else
	{
		return 0;
	}
}

sub getTypeHeader($)
{
	my $TypeInfo = $LibInfo{$Version}{$_[0]}{"info"};
	if($TypeInfo =~ /srcp[ ]*:[ ]*([0-9a-zA-Z\_\-\<\>\.\+]+):([0-9]+) /)
    {
        return ($1, $2);
    }
    else
    {
        return ();
    }
}

sub register_header($$$)
{
    my ($Destination, $Position, $LibVersion) = @_;
    return if(not $Destination);
    if($Destination=~/\A\// and not -f $Destination)
    {
        print "\nERROR: specified header \'$Destination\' does not exist\n";
        return;
    }
    my $Header_Name = get_FileName($Destination);
    $Headers{$LibVersion}{$Destination}{"Name"} = $Header_Name;
    $Headers{$LibVersion}{$Destination}{"Position"} = $Position;
    $Headers{$LibVersion}{$Destination}{"Identity"} = $Destination;
    $HeaderName_Destinations{$LibVersion}{$Header_Name}{$Destination} = 1;
}

sub headerSearch($)
{
    my $LibVersion = $_[0];
    foreach my $Dest (split("\n", $Descriptor{$LibVersion}{"Include_Paths"}))
    {
        $Dest =~ s/\A\s+|\s+\Z//g;
        next if(not $Dest);
        if(not -e $Dest)
        {
            print "ERROR: can't access \'$Dest\'\n";
        }
        $Header_Dependency{$LibVersion}{$Dest} = 1;
    }
    foreach my $Dest (split("\n", $Descriptor{$LibVersion}{"Headers"}))
    {#fill %Header_Dependency
        if(-d $Dest)
        {
            foreach my $Dir (cmd_find($Dest,"d",""))
            {
                chomp($Dir);
                $Header_Dependency{$LibVersion}{$Dir} = 1;
            }
        }
        elsif(-f $Dest)
        {
            $Header_Dependency{$LibVersion}{get_Dir($Dest)} = 1;
        }
    }
    my $Position = 0;
	foreach my $Dest (split("\n", $Descriptor{$LibVersion}{"Headers"}))
	{
        $Dest =~ s/\A\s+|\s+\Z//g;
        next if(not $Dest);
        if(is_header($Dest, $LibVersion))
        {
            register_header($Dest, $Position, $LibVersion);
            $Position += 1;
        }
        elsif(-d $Dest)
        {
            foreach my $Destination (sort {lc($a) cmp lc($b)} (cmd_find($Dest,"f","")))
            {
                chomp($Destination);
                next if(not is_header($Destination, $LibVersion));
                register_header($Destination, $Position, $LibVersion);
                $Position += 1;
            }
        }
        else
        {
            print "WARNING: \'$Dest\' was not found in the specified include paths\n";
            register_header($Dest, $Position, $LibVersion);
            $Position += 1;
        }
	}
    if(keys(%{$Headers{$LibVersion}})==1)
    {
        my $Destination = (keys(%{$Headers{$LibVersion}}))[0];
        $Headers{$LibVersion}{$Destination}{"Identity"} = $Headers{$LibVersion}{$Destination}{"Name"};
    }
    foreach my $Header_Name (keys(%{$HeaderName_Destinations{$LibVersion}}))
    {#set relative paths (for dublicates)
        if(keys(%{$HeaderName_Destinations{$LibVersion}{$Header_Name}})>1)
        {
            my $FirstDest = (keys(%{$HeaderName_Destinations{$LibVersion}{$Header_Name}}))[0];
            my $Prefix = get_Dir($FirstDest);
            while($Prefix =~ /\A(.+)\/[^\/]+\Z/)
            {
                my $NewPrefix = $1;
                my $Changes_Number = 0;
                my %Identity = ();
                foreach my $Dest (keys(%{$HeaderName_Destinations{$LibVersion}{$Header_Name}}))
                {
                    if($Dest =~ /\A$Prefix\/(.*)\Z/)
                    {
                        $Identity{$Dest} = $1;
                        $Changes_Number+=1;
                    }
                }
                if($Changes_Number eq keys(%{$HeaderName_Destinations{$LibVersion}{$Header_Name}}))
                {
                    foreach my $Dest (keys(%{$HeaderName_Destinations{$LibVersion}{$Header_Name}}))
                    {
                        $Headers{$LibVersion}{$Dest}{"Identity"} = $Identity{$Dest};
                    }
                    last;
                }
                $Prefix = $NewPrefix;
            }
        }
    }
    if(not keys(%{$Headers{$LibVersion}}))
    {
        print "ERROR: header files were not found\n";
        exit(1);
    }
}

sub get_FileName($)
{
    my $Path = $_[0];
    if($Path =~ /\A(.*\/)([^\/]*)\Z/)
    {
        return $2;
    }
    else
    {
        return $Path;
    }
}

sub get_Dir($)
{
    my $Path = $_[0];
    return "" if($Path =~ m*\A\./*);
    if($Path =~ /\A(.*)\/([^\/]*)\Z/)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub esc($)
{
	my $Str = $_[0];
	$Str =~ s/([()\[\]{}$ &'"`;,<>])/\\$1/g;
	return $Str;
}

sub getLocation($)
{
	my $TypeInfo = $LibInfo{$Version}{$_[0]}{"info"};
	if($TypeInfo =~ /srcp[ ]*:[ ]*([0-9a-zA-Z\_\-\<\>\.\+]+):([0-9]+) /)
    {
	    return ($1, $2);
    }
    else
    {
        return ();
    }
}

sub getHeader($)
{
    my $TypeInfo = $LibInfo{$Version}{$_[0]}{"info"};
    if($TypeInfo =~ /srcp[ ]*:[ ]*([0-9a-zA-Z\_\-\<\>\.\+]+):([0-9]+) /)
    {
        return $1;
    }
    else
    {
        return "";
    }
}

sub getLine($)
{
    my $TypeInfo = $LibInfo{$Version}{$_[0]}{"info"};
    if($TypeInfo =~ /srcp[ ]*:[ ]*([0-9a-zA-Z\_\-\<\>\.\+]+):([0-9]+) /)
    {
        return $2;
    }
    else
    {
        return "";
    }
}

sub getTypeType($$)
{
	my ($TypeDeclId, $TypeId) = @_;
	return "Const" if($LibInfo{$Version}{$TypeId}{"info"} =~ /qual[ ]*:[ ]*c / and $LibInfo{$Version}{$TypeId}{"info"} =~ /unql[ ]*:[ ]*\@/);
	return "Typedef" if($LibInfo{$Version}{$TypeId}{"info"} =~ /unql[ ]*:/ and $LibInfo{$Version}{$TypeId}{"info"} !~ /qual[ ]*:/);
	return "Volatile" if($LibInfo{$Version}{$TypeId}{"info"} =~ /qual[ ]*:[ ]*v / and $LibInfo{$Version}{$TypeId}{"info"} =~ /unql[ ]*:[ ]*\@/);
	return "Restrict" if($LibInfo{$Version}{$TypeId}{"info"} =~ /qual[ ]*:[ ]*r / and $LibInfo{$Version}{$TypeId}{"info"} =~ /unql[ ]*:[ ]*\@/);
	my $TypeType = getTypeTypeByTypeId($TypeId);
	if($TypeType eq "Struct")
	{
		if($TypeDeclId and $LibInfo{$Version}{$TypeDeclId}{"info_type"} eq "template_decl")
        {
            return "Template";
        }
        else
        {
            return "Struct";
        }
	}
	else
	{
		return $TypeType;
	}
	
}

sub getTypeTypeByTypeId($)
{
	my $TypeId = $_[0];
	my $TypeType = $LibInfo{$Version}{$TypeId}{"info_type"};
	if($TypeType =~ /integer_type|real_type|boolean_type|void_type|complex_type/)
	{
		return "Intrinsic";
	}
	elsif(isFuncPtr($TypeId))
	{
		return "FuncPtr";
	}
	elsif($TypeType eq "pointer_type")
	{
		return "Pointer";
	}
	elsif($TypeType eq "reference_type")
	{
		return "Ref";
	}
	elsif($TypeType eq "union_type")
	{
		return "Union";
	}
	elsif($TypeType eq "enumeral_type")
	{
		return "Enum";
	}
	elsif($TypeType eq "record_type")
	{
		return "Struct";
	}
	elsif($TypeType eq "array_type")
	{
		return "Array";
	}
	elsif($TypeType eq "complex_type")
	{
		return "Intrinsic";
	}
	elsif($TypeType eq "function_type")
	{
		return "FunctionType";
	}
    elsif($TypeType eq "method_type")
    {
        return "MethodType";
    }
	else
	{
		return "Unknown";
	}
}

sub getNameByInfo($)
{
	my $TypeInfo = $LibInfo{$Version}{$_[0]}{"info"};
	return "" if($TypeInfo !~ /name[ ]*:[ ]*@([0-9]+) /);
	my $TypeNameInfoId = $1;
	return "" if($LibInfo{$Version}{$TypeNameInfoId}{"info"} !~ /strg[ ]*:[ ]*(.*)[ ]+lngt/);
	my $TypeName = $1;
	$TypeName =~ s/[ ]+\Z//g;
	return $TypeName;
}

sub getFuncShortName($)
{
	my $FuncInfo = $LibInfo{$Version}{$_[0]}{"info"};
    if($FuncInfo =~ / operator /)
    {
        if($FuncInfo =~ /note[ ]*:[ ]*conversion /)
        {
            return "operator ".get_TypeName($FuncDescr{$Version}{$_[0]}{"Return"});
        }
        else
        {
            return "" if($FuncInfo !~ / operator[ ]+([a-zA-Z]+) /);
            return "operator".$Operator_Indication{$1};
        }
    }
    else
    {
        return "" if($FuncInfo !~ /name[ ]*:[ ]*@([0-9]+) /);
        my $FuncNameInfoId = $1;
        return "" if($LibInfo{$Version}{$FuncNameInfoId}{"info"} !~ /strg[ ]*:[ ]*([^ ]*)[ ]+lngt/);
        return $1;
    }
}

sub getFuncMnglName($)
{
	my $FuncInfo = $LibInfo{$Version}{$_[0]}{"info"};
	return "" if($FuncInfo !~ /mngl[ ]*:[ ]*@([0-9]+) /);
	my $FuncMnglNameInfoId = $1;
	return "" if($LibInfo{$Version}{$FuncMnglNameInfoId}{"info"} !~ /strg[ ]*:[ ]*([^ ]*)[ ]+/);
	my $FuncMnglName = $1;
	$FuncMnglName =~ s/[ ]+\Z//g;
	return $FuncMnglName;
}

sub getFuncReturn($)
{
	my $FuncInfo = $LibInfo{$Version}{$_[0]}{"info"};
	return "" if($FuncInfo !~ /type[ ]*:[ ]*@([0-9]+) /);
	my $FuncTypeInfoId = $1;
	return "" if($LibInfo{$Version}{$FuncTypeInfoId}{"info"} !~ /retn[ ]*:[ ]*@([0-9]+) /);
	my $FuncReturnTypeId = $1;
	if($TypeDescr{$Version}{getTypeDeclId($FuncReturnTypeId)}{$FuncReturnTypeId}{"Type"} eq "Restrict")
	{#delete restrict spec
		$FuncReturnTypeId = getRestrictBase($FuncReturnTypeId);
	}
	return $FuncReturnTypeId;
}

sub getFuncOrig($)
{
	my $FuncInfo = $LibInfo{$Version}{$_[0]}{"info"};
	if($FuncInfo =~ /orig[ ]*:[ ]*@([0-9]+) /)
    {
	    return $1;
    }
    else
    {
        return $_[0];
    }
}

sub unmangleArray(@)
{
    if($#_>$MAX_COMMAND_LINE_ARGUMENTS)
    {
        my @Half = splice(@_, 0, ($#_+1)/2);
        return (unmangleArray(@Half), unmangleArray(@_))
    }
    else
    {
        my $UnmangleCommand = "c++filt ".join(" ", @_);
        return split("\n", `$UnmangleCommand`);
    }
}

sub get_Signature($$)
{
	my ($Interface, $LibVersion) = @_;
    return $Cache{"get_Signature"}{$Interface}{$LibVersion} if($Cache{"get_Signature"}{$Interface}{$LibVersion});
    my ($MnglName, $SymbolVersion) = ($Interface, "");
    if($Interface =~ /\A([^@]+)[\@]+([^@]+)\Z/)
    {
        ($MnglName, $SymbolVersion) = ($1, $2);
    }
    if($MnglName =~ /\A(_ZGV|_ZTI|_ZTS|_ZTT|_ZTV|_ZThn|_ZTv0_n)/)
    {
        $Cache{"get_Signature"}{$Interface}{$LibVersion} = $tr_name{$MnglName}.(($SymbolVersion)?"\@".$SymbolVersion:"");
        return $Cache{"get_Signature"}{$Interface}{$LibVersion};
    }
    if(not $CompleteSignature{$LibVersion}{$Interface})
    {
        if($Interface =~ /\A_Z/)
        {
            $Cache{"get_Signature"}{$Interface}{$LibVersion} = $tr_name{$MnglName}.(($SymbolVersion)?"\@".$SymbolVersion:"");
            return $Cache{"get_Signature"}{$Interface}{$LibVersion};
        }
        else
        {
            $Cache{"get_Signature"}{$Interface}{$LibVersion} = $Interface;
            return $Interface;
        }
    }
    my ($Func_Signature, @Param_Types_FromUnmangledName) = ();
    my $ShortName = $CompleteSignature{$LibVersion}{$Interface}{"ShortName"};
    if($Interface =~ /\A_Z/)
    {
        if($CompleteSignature{$LibVersion}{$Interface}{"Class"})
        {
            $Func_Signature = get_TypeName($CompleteSignature{$LibVersion}{$Interface}{"Class"}, $LibVersion)."::".(($CompleteSignature{$LibVersion}{$Interface}{"Destructor"})?"~":"").$ShortName;
        }
        else
        {
            $Func_Signature = $ShortName;
        }
        @Param_Types_FromUnmangledName = get_Signature_Parts($tr_name{$MnglName}, 0);
    }
    else
    {
        $Func_Signature = $MnglName;
    }
    my @ParamArray = ();
    foreach my $Pos (sort {int($a) <=> int($b)} keys(%{$CompleteSignature{$LibVersion}{$Interface}{"Param"}}))
    {
        next if($Pos eq "");
        my $ParamTypeId = $CompleteSignature{$LibVersion}{$Interface}{"Param"}{$Pos}{"type"};
        my $ParamTypeName = $TypeDescr{$LibVersion}{$Tid_TDid{$LibVersion}{$ParamTypeId}}{$ParamTypeId}{"Name"};
        $ParamTypeName = $Param_Types_FromUnmangledName[$Pos] if(not $ParamTypeName);
        if(my $ParamName = $CompleteSignature{$LibVersion}{$Interface}{"Param"}{$Pos}{"name"})
        {
            if($ParamTypeName=~/\([*]+\)/)
            {
                $ParamTypeName=~s/\(([*]+)\)/\($1$ParamName\)/;
                push(@ParamArray, $ParamTypeName);
            }
            else
            {
                push(@ParamArray, $ParamTypeName." ".$ParamName);
            }
        }
        else
        {
            push(@ParamArray, $ParamTypeName);
        }
    }
    if(not $CompleteSignature{$LibVersion}{$Interface}{"Data"})
    {
        if($Interface=~/\A_Z/)
        {
            if($CompleteSignature{$LibVersion}{$Interface}{"Constructor"})
            {
                if($Interface=~/C1/)
                {
                    $Func_Signature .= " [in-charge]";
                }
                elsif($Interface=~/C2/)
                {
                    $Func_Signature .= " [not-in-charge]";
                }
            }
            elsif($CompleteSignature{$LibVersion}{$Interface}{"Destructor"})
            {
                if($Interface=~/D1/)
                {
                    $Func_Signature .= " [in-charge]";
                }
                elsif($Interface=~/D2/)
                {
                    $Func_Signature .= " [not-in-charge]";
                }
                elsif($Interface=~/D0/)
                {
                    $Func_Signature .= " [in-charge-deleting]";
                }
            }
        }
        $Func_Signature .= " (".join(", ", @ParamArray).")";
    }
    if($Interface=~/\A_ZNK/)
    {
        $Func_Signature .= " const";
    }
    $Func_Signature .= "\@".$SymbolVersion if($SymbolVersion);
    $Cache{"get_Signature"}{$Interface}{$LibVersion} = $Func_Signature;
    return $Func_Signature;
}

sub getVarNameByAttr($)
{
	my $FuncInfoId = $_[0];
	my $VarName;
	return "" if(not $FuncDescr{$Version}{$FuncInfoId}{"ShortName"});
	if($FuncDescr{$Version}{$FuncInfoId}{"Class"})
	{
		$VarName .= $TypeDescr{$Version}{getTypeDeclId($FuncDescr{$Version}{$FuncInfoId}{"Class"})}{$FuncDescr{$Version}{$FuncInfoId}{"Class"}}{"Name"};
		$VarName .= "::";
	}
	$VarName .= $FuncDescr{$Version}{$FuncInfoId}{"ShortName"};
	return $VarName;
}

sub mangleFuncName($)
{
	my $FuncId = $_[0];
}

sub getFuncType($)
{
	my $FuncInfo = $LibInfo{$Version}{$_[0]}{"info"};
	return "" if($FuncInfo !~ /type[ ]*:[ ]*@([0-9]+) /);
	my $FuncTypeInfoId = $1;
	my $FunctionType = $LibInfo{$Version}{$FuncTypeInfoId}{"info_type"};
	if($FunctionType eq "method_type")
	{
		return "Method";
	}
	elsif($FunctionType eq "function_type")
	{
		return "Function";
	}
	else
	{
		return $FunctionType;
	}
}

sub getFuncTypeId($)
{
    my $FuncInfo = $LibInfo{$Version}{$_[0]}{"info"};
    if($FuncInfo =~ /type[ ]*:[ ]*@([0-9]+)( |\Z)/)
    {
        return $1;
    }
    else
    {
        return 0;
    }
}

sub isNotAnon($)
{
	return (not isAnon($_[0]));
}

sub isAnon($)
{
	return (($_[0] =~ m/\.\_[0-9]+/) or ($_[0] =~ m/anon-/));
}

sub unmangled_Compact($$)
#Removes all non-essential (for C++ language) whitespace from a string.  If 
#the whitespace is essential it will be replaced with exactly one ' ' 
#character. Works correctly only for unmangled names.
#If level > 1 is supplied, can relax its intent to compact the string.
{
  my $result=$_[0];
  my $level = $_[1] || 1;
  my $o1 = ($level>1)?' ':'';
  #First, we reduce all spaces that we can
  my $coms='[-()<>:*&~!|+=%@~"?.,/[^'."']";
  my $coms_nobr='[-()<:*&~!|+=%@~"?.,'."']";
  my $clos='[),;:\]]';
  $result=~s/^\s+//gm;
  $result=~s/\s+$//gm;
  $result=~s/((?!\n)\s)+/ /g;
  $result=~s/(\w+)\s+($coms+)/$1$o1$2/gm;
  #$result=~s/(\w)(\()/$1$o1$2/gm if $o1;
  $result=~s/($coms+)\s+(\w+)/$1$o1$2/gm;
  $result=~s/(\()\s+(\w)/$1$2/gm if $o1;
  $result=~s/(\w)\s+($clos)/$1$2/gm;
  $result=~s/($coms+)\s+($coms+)/$1 $2/gm;
  $result=~s/($coms_nobr+)\s+($coms+)/$1$o1$2/gm;
  $result=~s/($coms+)\s+($coms_nobr+)/$1$o1$2/gm;
  #don't forget about >> and <:.  In unmangled names global-scope modifier 
  #is not used, so <: will always be a digraph and requires no special treatment.
  #We also try to remove other parts that are better to be removed here than in other places
  #double-cv
  $result=~s/\bconst\s+const\b/const/gm;
  $result=~s/\bvolatile\s+volatile\b/volatile/gm;
  $result=~s/\bconst\s+volatile\b\s+const\b/const volatile/gm;
  $result=~s/\bvolatile\s+const\b\s+volatile\b/const volatile/gm;
  #Place cv in proper order
  $result=~s/\bvolatile\s+const\b/const volatile/gm;
  return $result;
}

sub unmangled_PostProcess($)
{
  my $result = $_[0];
  #s/\bunsigned int\b/unsigned/g;
  $result =~ s/\bshort unsigned int\b/unsigned short/g;
  $result =~ s/\bshort int\b/short/g;
  $result =~ s/\blong long unsigned int\b/unsigned long long/g;
  $result =~ s/\blong unsigned int\b/unsigned long/g;
  $result =~ s/\blong long int\b/long long/g;
  $result =~ s/\blong int\b/long/g;
  $result =~ s/\)const\b/\) const/g;
  $result =~ s/\blong long unsigned\b/unsigned long long/g;
  $result =~ s/\blong unsigned\b/unsigned long/g;
  return $result;
}

# From libtodb2/libtodb.pm
# Trim string spaces.
sub trim($)
{
        my $string = shift;
        $string =~ s/^\s+//;
        $string =~ s/\s+$//;
        return $string;
}

sub correctName($)
{#type name correction
    my $CorrectName = $_[0];
    $CorrectName = unmangled_Compact($CorrectName, 1);
    $CorrectName = unmangled_PostProcess($CorrectName);
	return $CorrectName;
}

sub getDump_AllInOne()
{
    return if(not keys(%Headers));
    `mkdir -p temp && rm -f temp/*`;
	my $Header_Depend = "";
    foreach my $Dep (sort {length($a) <=> length($b)} keys(%{$Header_Dependency{$Version}}))
    {
        $Header_Depend .= " -I".esc($Dep);
    }
    my $Lib_VersionName = esc($TargetLibraryName)."_v".$Version;
    open(LIB_HEADER, ">temp/$TargetLibraryName"."_v$Version.h");
    foreach my $Preamble_Header (sort {int($Include_Preamble{$Version}{$a}{"Position"})<=>int($Include_Preamble{$Version}{$b}{"Position"})} keys(%{$Include_Preamble{$Version}}))
    {
        print LIB_HEADER "#include <$Preamble_Header>\n";
    }
	foreach my $Destination (sort {int($Headers{$Version}{$a}{"Position"})<=>int($Headers{$Version}{$b}{"Position"})} keys(%{$Headers{$Version}}))
	{
        my $HName = get_FileName($Destination);
        next if($Include_Preamble{$Version}{$Destination} or ($Include_Preamble{$Version}{$HName} and $HeaderName_Destinations{$Version}{$HName} and keys(%{$HeaderName_Destinations{$Version}{$HName}})==1));
		print LIB_HEADER "#include <$Destination>\n";
	}
	close(LIB_HEADER);
	system("g++ >".esc($ERR_PATH{$Version})." 2>&1 -fdump-translation-unit temp/$Lib_VersionName.h $CompilerOptions{$Version} $Header_Depend");
    if($?)
    {
        print "WARNING: some errors have occured while compiling header(s), fix it first!\nyou can see compilation errors in the file \'$ERR_PATH{$Version}\'\n";
    }
    $ConstantsSrc{$Version} = cmd_preprocessor("temp/$Lib_VersionName.h", $CompilerOptions{$Version}." ".$Header_Depend, "define\\ \\|undef\\ \\|#[ ]\\+[0-9]\\+ \".*\"");
    system("mkdir", "-p", "header_compile_errors/$TargetLibraryName/temp/");
    system("cp", "-f", "temp/$TargetLibraryName"."_v$Version.h", "header_compile_errors/$TargetLibraryName/temp/");
	return (split("\n", `find . -maxdepth 1 -name "$Lib_VersionName\.h*\.tu"`))[0];
}

sub getDump_Separately($)
{
    `mkdir -p temp`;
    `rm -f temp/*`;
	my $Destination = $_[0];
	my $Header_Depend = "";
    foreach my $Dep (sort {length($a) <=> length($b)} keys(%{$Header_Dependency{$Version}}))
    {
        $Header_Depend .= " -I".esc($Dep);
    }
    my $Lib_VersionName = esc($TargetLibraryName)."_v".$Version;
	open(LIB_HEADER, ">temp/$TargetLibraryName"."_v$Version.h");
    foreach my $Preamble_Header (sort {int($Include_Preamble{$Version}{$a}{"Position"})<=>int($Include_Preamble{$Version}{$b}{"Position"})} keys(%{$Include_Preamble{$Version}}))
    {
        print LIB_HEADER "#include <$Preamble_Header>\n";
    }
    my $Destination_HName = get_FileName($Destination);
    if(not ( $Include_Preamble{$Version}{$Destination} or ($Include_Preamble{$Version}{$Destination_HName} and $HeaderName_Destinations{$Version}{$Destination_HName} and keys(%{$HeaderName_Destinations{$Version}{$Destination_HName}})==1) ))
    {
        print LIB_HEADER "#include <$Destination>\n";
    }
	close(LIB_HEADER);
	system("g++ >>".esc($ERR_PATH{$Version})." 2>&1 -fdump-translation-unit temp/$Lib_VersionName.h $CompilerOptions{$Version} $Header_Depend");
    if($?)
    {
        $HeaderCompileError{get_FileName($Destination)} = 1;
    }
    $ConstantsSrc{$Version} .= cmd_preprocessor("temp/$Lib_VersionName.h", $CompilerOptions{$Version}." ".$Header_Depend, "define\\ \\|undef\\ \\|#[ ]\\+[0-9]\\+ \".*\"");
	return (split("\n", `find . -maxdepth 1 -name "$Lib_VersionName\.h*\.tu"`))[0];
}

sub cmd_file($)
{
    my $Path = $_[0];
    return "" if(not $Path or not -e $Path);
    my $Cmd = "file ".esc($Path);
    my $Cmd_Out = `$Cmd`;
    return $Cmd_Out;
}

sub cmd_preprocessor($$$)
{
    my ($Path, $AddOpt, $Grep) = @_;
    return "" if(not $Path or not -f $Path);
    my $Cmd = "g++ -dD -E -x c++-header ".esc($Path)." 2>/dev/null $AddOpt";
    if($Grep)
    {
        $Cmd .= " | grep \"$Grep\"";
    }
    my $Cmd_Out = `$Cmd`;
    return $Cmd_Out;
}

sub cmd_cat($$)
{
    my ($Path, $Grep) = @_;
    return "" if(not $Path or not -e $Path);
    my $Cmd = "cat ".esc($Path);
    if($Grep)
    {
        $Cmd .= " | grep \"$Grep\"";
    }
    my $Cmd_Out = `$Cmd`;
    return $Cmd_Out;
}

sub cmd_find($$$)
{
    my ($Path, $Type, $Name) = @_;
    return () if(not $Path or not -e $Path);
    my $Cmd = "find ".esc($Path);
    if($Type)
    {
        $Cmd .= " -type $Type";
    }
    if($Name)
    {
        $Cmd .= " -name \"$Name\"";
    }
    return split("\n", `$Cmd`);
}

sub cmd_tar($)
{
    my $Path = $_[0];
    return "" if(not $Path or not -e $Path);
    my $Cmd = "tar -xvzf ".esc($Path);
    my $Cmd_Out = `$Cmd`;
    return $Cmd_Out;
}

sub is_header($$)
{
	my ($Destination, $LibVersion) = @_;
    return 0 if(-d $Destination);
    return 1 if($Destination =~ /\.(h|hh|hhh|hpp|tcc)\Z/i);
    return 0 if($Destination =~ /\.\w+\Z/i);#cpp|c|gch|tu|fs|pas
    if($Destination =~ /\A\//)
    {
        return (cmd_file($Destination) =~ /:[ ]*ASCII C[\+]* program text/);
    }
    else
    {
        foreach my $Prefix (keys(%{$Header_Dependency{$LibVersion}}))
        {
            my $Path = $Prefix."/".$Destination;
            if(-f $Path)
            {
                if(cmd_file($Path) =~ /:[ ]*ASCII C[\+]* program text/)
                {
                    return 1;
                }
            }
        }
        return 0;
    }
}

sub parseHeaders_AllInOne($)
{
    $Version = $_[0];
    print "checking header(s) ".$Descriptor{$Version}{"Version"}." ...\n";
    system("mkdir", "-p", "header_compile_errors/".$TargetLibraryName);
    system("rm", "-fr", $ERR_PATH{$Version});
	my $DumpPath = getDump_AllInOne();
	if(not $DumpPath)
	{
		print "\nERROR: can't create gcc syntax tree for header(s)\nyou can see compilation errors in the file '$ERR_PATH{$Version}'\n";
		exit(1);
	}
	getInfo($DumpPath);
    parse_constants();
    system("rm", "-fr", "temp");
}

sub parseHeader($)
{
	my $Destination = $_[0];
	my $DumpPath = getDump_Separately($Destination);
	if(not $DumpPath)
	{
		print "ERROR: can't create gcc syntax tree for header\nyou can see compilation errors in the file '$ERR_PATH{$Version}'\n";
		exit(1);
	}
	getInfo($DumpPath);
    parse_constants();
    system("rm", "-fr", "temp");
}

sub is_in_library($$)
{
    my ($MnglName, $LibVersion) = @_;
    return ($Interface_Library{$LibVersion}{$MnglName} or ($SymVer{$LibVersion}{$MnglName} and $Interface_Library{$LibVersion}{$SymVer{$LibVersion}{$MnglName}}));
}

sub prepareInterfaces($)
{
	my $LibVersion = $_[0];
    my (@MnglNames, @UnMnglNames) = ();
    if($CheckHeadersOnly)
    {
        foreach my $FuncInfoId (sort keys(%{$FuncDescr{$LibVersion}}))
        {
            if($FuncDescr{$LibVersion}{$FuncInfoId}{"MnglName"} =~ /\A_Z/)
            {
                push(@MnglNames, $FuncDescr{$LibVersion}{$FuncInfoId}{"MnglName"});
            }
        }
        if($#MnglNames > -1)
        {
            @UnMnglNames = reverse(unmangleArray(@MnglNames));
            foreach my $FuncInfoId (sort keys(%{$FuncDescr{$LibVersion}}))
            {
                if($FuncDescr{$LibVersion}{$FuncInfoId}{"MnglName"} =~ /\A_Z/)
                {
                    my $UnmangledName = pop(@UnMnglNames);
                    $tr_name{$FuncDescr{$LibVersion}{$FuncInfoId}{"MnglName"}} = $UnmangledName;
                }
            }
        }
    }
    my (%NotMangled_Int, %Mangled_Int) = ();
    foreach my $FuncInfoId (keys(%{$FuncDescr{$LibVersion}}))
    {
        my $MnglName = $FuncDescr{$LibVersion}{$FuncInfoId}{"MnglName"};
        if($MnglName=~/\A_Z/)
        {
            $Mangled_Int{$FuncInfoId} = $MnglName;
        }
        else
        {
            $NotMangled_Int{$FuncInfoId} = $MnglName;
        }
        next if(not $MnglName or not is_in_library($MnglName, $LibVersion) and not $CheckHeadersOnly);
        next if($MnglName =~ /\A_Z/ and $tr_name{$MnglName} =~ /\.\_[0-9]/);
        next if(not $FuncDescr{$LibVersion}{$FuncInfoId}{"Header"});
        %{$CompleteSignature{$LibVersion}{$MnglName}} = %{$FuncDescr{$LibVersion}{$FuncInfoId}};
        #interface and its symlink have same signatures
        if($SymVer{$LibVersion}{$MnglName})
        {
            %{$CompleteSignature{$LibVersion}{$SymVer{$LibVersion}{$MnglName}}} = %{$FuncDescr{$LibVersion}{$FuncInfoId}};
        }
        delete($FuncDescr{$LibVersion}{$FuncInfoId});
    }
    if(keys(%Mangled_Int))
    {
        foreach my $Interface_Id (keys(%NotMangled_Int))
        {
            delete($CompleteSignature{$LibVersion}{$NotMangled_Int{$Interface_Id}});
        }
    }
}

my %UsedType;
sub cleanData($)
{
    my $LibVersion = $_[0];
    foreach my $FuncInfoId (keys(%{$FuncDescr{$LibVersion}}))
    {
        my $MnglName = $FuncDescr{$LibVersion}{$FuncInfoId}{"MnglName"};
        if(not $MnglName or not is_in_library($MnglName, $LibVersion) and not $CheckHeadersOnly)
        {
            delete($FuncDescr{$LibVersion}{$FuncInfoId});
            next;
        }
        if(defined $InterfacesListPath and not $InterfacesList{$MnglName})
        {
            delete($FuncDescr{$LibVersion}{$FuncInfoId});
            next;
        }
        if(defined $AppPath and not $InterfacesList_App{$MnglName})
        {
            delete($FuncDescr{$LibVersion}{$FuncInfoId});
            next;
        }
        my %FuncInfo = %{$FuncDescr{$LibVersion}{$FuncInfoId}};
        detect_TypeUsing($Tid_TDid{$LibVersion}{$FuncInfo{"Return"}}, $FuncInfo{"Return"}, $LibVersion);
        detect_TypeUsing($Tid_TDid{$LibVersion}{$FuncInfo{"Class"}}, $FuncInfo{"Class"}, $LibVersion);
        foreach my $Param_Pos (keys(%{$FuncInfo{"Param"}}))
        {
            my $Param_TypeId = $FuncInfo{"Param"}{$Param_Pos}{"type"};
            detect_TypeUsing($Tid_TDid{$LibVersion}{$Param_TypeId}, $Param_TypeId, $LibVersion);
        }
    }
    foreach my $TDid (keys(%{$TypeDescr{$LibVersion}}))
    {
        foreach my $Tid (keys(%{$TypeDescr{$LibVersion}{$TDid}}))
        {
            if(not $UsedType{$LibVersion}{$TDid}{$Tid})
            {
                delete($TypeDescr{$LibVersion}{$TDid}{$Tid});
                if(not keys(%{$TypeDescr{$LibVersion}{$TDid}}))
                {
                    delete($TypeDescr{$LibVersion}{$TDid});
                }
                delete($Tid_TDid{$LibVersion}{$Tid}) if($Tid_TDid{$LibVersion}{$Tid} eq $TDid);
            }
        }
    }
}

sub detect_TypeUsing($$$)
{
    my ($TypeDeclId, $TypeId, $LibVersion) = @_;
    return if($UsedType{$LibVersion}{$TypeDeclId}{$TypeId});
    my %Type = get_Type($TypeDeclId, $TypeId, $LibVersion);
    if($Type{"Type"} =~ /Struct|Union|Class|FuncPtr|Enum/)
    {
        $UsedType{$LibVersion}{$TypeDeclId}{$TypeId} = 1;
        foreach my $Memb_Pos (keys(%{$Type{"Memb"}}))
        {
            my $Member_TypeId = $Type{"Memb"}{$Memb_Pos}{"type"};
            detect_TypeUsing($Tid_TDid{$LibVersion}{$Member_TypeId}, $Member_TypeId, $LibVersion);
        }
        if($Type{"Type"} eq "FuncPtr")
        {
            my $ReturnType = $Type{"Return"};
            detect_TypeUsing($Tid_TDid{$LibVersion}{$ReturnType}, $ReturnType, $LibVersion);
        }
    }
    elsif($Type{"Type"} =~ /Const|Pointer|Ref|Volatile|Restrict|Array|/)
    {
        $UsedType{$LibVersion}{$TypeDeclId}{$TypeId} = 1;
        detect_TypeUsing($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"}, $LibVersion);
    }
    elsif($Type{"Type"} eq "Intrinsic")
    {
        $UsedType{$LibVersion}{$TypeDeclId}{$TypeId} = 1;
    }
    else
    {
        delete($TypeDescr{$LibVersion}{$TypeDeclId}{$TypeId});
        if(not keys(%{$TypeDescr{$LibVersion}{$TypeDeclId}}))
        {
            delete($TypeDescr{$LibVersion}{$TypeDeclId});
        }
        delete($Tid_TDid{$LibVersion}{$TypeId}) if($Tid_TDid{$LibVersion}{$TypeId} eq $TypeDeclId);
    }
}

sub initializeClassVirtFunc($)
{
    my $LibVersion = $_[0];
    foreach my $Interface (keys(%{$CompleteSignature{$LibVersion}}))
    {
        if($CompleteSignature{$LibVersion}{$Interface}{"Virt"})
        {
            my $ClassName = $TypeDescr{$LibVersion}{$Tid_TDid{$LibVersion}{$CompleteSignature{$LibVersion}{$Interface}{"Class"}}}{$CompleteSignature{$LibVersion}{$Interface}{"Class"}}{"Name"};
            $ClassVirtFunc{$LibVersion}{$ClassName}{$Interface} = 1;
            $ClassIdVirtFunc{$LibVersion}{$CompleteSignature{$LibVersion}{$Interface}{"Class"}}{$Interface} = 1;
            $ClassId{$LibVersion}{$ClassName} = $CompleteSignature{$LibVersion}{$Interface}{"Class"};
        }
    }
}

sub checkVirtFuncRedefinitions($)
{
    my $LibVersion = $_[0];
    foreach my $Class_Name (keys(%{$ClassVirtFunc{$LibVersion}}))
    {
        $CheckedTypes{$Class_Name} = 1;
        foreach my $VirtFuncName (keys(%{$ClassVirtFunc{$LibVersion}{$Class_Name}}))
        {
            $CompleteSignature{$LibVersion}{$VirtFuncName}{"VirtualRedefine"} = find_virtual_method_in_base_classes($VirtFuncName, $ClassId{$LibVersion}{$Class_Name}, $LibVersion);
        }
    }
}

sub setVirtFuncPositions($)
{
    my $LibVersion = $_[0];
    foreach my $Class_Name (keys(%{$ClassVirtFunc{$LibVersion}}))
    {
        $CheckedTypes{$Class_Name} = 1;
        my $Position = 0;
        foreach my $VirtFuncName (sort {int($CompleteSignature{$LibVersion}{$a}{"Line"}) <=> int($CompleteSignature{$LibVersion}{$b}{"Line"})} keys(%{$ClassVirtFunc{$LibVersion}{$Class_Name}}))
        {
            if($ClassVirtFunc{1}{$Class_Name}{$VirtFuncName} and $ClassVirtFunc{2}{$Class_Name}{$VirtFuncName} and not $CompleteSignature{1}{$VirtFuncName}{"VirtualRedefine"} and not $CompleteSignature{2}{$VirtFuncName}{"VirtualRedefine"})
            {
                $CompleteSignature{$LibVersion}{$VirtFuncName}{"Position"} = $Position;
                $Position += 1;
            }
        }
    }
}

sub check_VirtualTable($$)
{
    my ($TargetFunction, $LibVersion) = @_;
    my $Class_Id = $CompleteSignature{$LibVersion}{$TargetFunction}{"Class"};
    my $Class_DId = $Tid_TDid{$LibVersion}{$Class_Id};
    my %Class_Type = get_Type($Class_DId, $Class_Id, $LibVersion);
    $CheckedTypes{$Class_Type{"Name"}} = 1;
    foreach my $VirtFuncName (keys(%{$ClassVirtFunc{2}{$Class_Type{"Name"}}}))
    {#Added
        if($ClassId{1}{$Class_Type{"Name"}} and not $ClassVirtFunc{1}{$Class_Type{"Name"}}{$VirtFuncName} and $AddedInt{$VirtFuncName})
        {
            if($CompleteSignature{2}{$VirtFuncName}{"VirtualRedefine"})
            {
                if($TargetFunction eq $VirtFuncName)
                {
                    my $BaseClass_Id = $CompleteSignature{2}{$CompleteSignature{2}{$VirtFuncName}{"VirtualRedefine"}}{"Class"};
                    my %BaseClass_Type = get_Type($Tid_TDid{2}{$BaseClass_Id}, $BaseClass_Id, 2);
                    my $BaseClass_Name = $BaseClass_Type{"Name"};
                    %{$CompatProblems{$TargetFunction}{"Virtual_Function_Redefinition"}{$tr_name{$CompleteSignature{2}{$VirtFuncName}{"VirtualRedefine"}}}}=(
                        "Type_Name"=>$Class_Type{"Name"},
                        "Type_Type"=>$Class_Type{"Type"},
                        "Header"=>$CompleteSignature{2}{$TargetFunction}{"Header"},
                        "Line"=>$CompleteSignature{2}{$TargetFunction}{"Line"},
                        "Target"=>$tr_name{$CompleteSignature{2}{$VirtFuncName}{"VirtualRedefine"}},
                        "Signature"=>$tr_name{$TargetFunction},
                        "Old_Value"=>$tr_name{$CompleteSignature{2}{$VirtFuncName}{"VirtualRedefine"}},
                        "New_Value"=>$tr_name{$TargetFunction},
                        "Old_SoName"=>$Interface_Library{1}{$TargetFunction},
                        "New_SoName"=>$Interface_Library{2}{$TargetFunction}  );
                }
            }
            elsif($TargetFunction ne $VirtFuncName)
            {
                %{$CompatProblems{$TargetFunction}{"Added_Virtual_Function"}{$tr_name{$VirtFuncName}}}=(
                "Type_Name"=>$Class_Type{"Name"},
                "Type_Type"=>$Class_Type{"Type"},
                "Header"=>$Class_Type{"Header"},
                "Line"=>$Class_Type{"Line"},
                "Target"=>$tr_name{$VirtFuncName},
                "Signature"=>$tr_name{$TargetFunction},
                "Old_SoName"=>$Interface_Library{1}{$TargetFunction},
                "New_SoName"=>$Interface_Library{2}{$TargetFunction}  );
            }
        }
    }
    foreach my $VirtFuncName (keys(%{$ClassVirtFunc{1}{$Class_Type{"Name"}}}))
    {#Withdrawn
        if($ClassId{2}{$Class_Type{"Name"}} and not $ClassVirtFunc{2}{$Class_Type{"Name"}}{$VirtFuncName} and $WithdrawnInt{$VirtFuncName})
        {
            if($CompleteSignature{1}{$VirtFuncName}{"VirtualRedefine"})
            {
                if($TargetFunction eq $VirtFuncName)
                {
                    my $BaseClass_Id = $CompleteSignature{1}{$CompleteSignature{1}{$VirtFuncName}{"VirtualRedefine"}}{"Class"};
                    my $BaseClass_Name = $TypeDescr{1}{$Tid_TDid{1}{$BaseClass_Id}}{$BaseClass_Id}{"Name"};
                    %{$CompatProblems{$TargetFunction}{"Virtual_Function_Redefinition_B"}{$tr_name{$CompleteSignature{1}{$VirtFuncName}{"VirtualRedefine"}}}}=(
                        "Type_Name"=>$Class_Type{"Name"},
                        "Type_Type"=>$Class_Type{"Type"},
                        "Header"=>$CompleteSignature{2}{$TargetFunction}{"Header"},
                        "Line"=>$CompleteSignature{2}{$TargetFunction}{"Line"},
                        "Target"=>$tr_name{$CompleteSignature{1}{$VirtFuncName}{"VirtualRedefine"}},
                        "Signature"=>$tr_name{$TargetFunction},
                        "Old_Value"=>$tr_name{$TargetFunction},
                        "New_Value"=>$tr_name{$CompleteSignature{1}{$VirtFuncName}{"VirtualRedefine"}},
                        "Old_SoName"=>$Interface_Library{1}{$TargetFunction},
                        "New_SoName"=>$Interface_Library{2}{$TargetFunction}  );
                }
            }
            else
            {
                %{$CompatProblems{$TargetFunction}{"Withdrawn_Virtual_Function"}{$tr_name{$VirtFuncName}}}=(
                "Type_Name"=>$Class_Type{"Name"},
                "Type_Type"=>$Class_Type{"Type"},
                "Header"=>$Class_Type{"Header"},
                "Line"=>$Class_Type{"Line"},
                "Target"=>$tr_name{$VirtFuncName},
                "Signature"=>$tr_name{$TargetFunction},
                "Old_SoName"=>$Interface_Library{1}{$TargetFunction},
                "New_SoName"=>$Interface_Library{2}{$TargetFunction}  );
            }
        }
    }
}

sub find_virtual_method_in_base_classes($$$)
{
    my ($VirtFuncName, $Checked_ClassId, $LibVersion) = @_;
    foreach my $BaseClass_Id (keys(%{$TypeDescr{$LibVersion}{$Tid_TDid{$LibVersion}{$Checked_ClassId}}{$Checked_ClassId}{"BaseClass"}}))
    {
        my $VirtMethodInClass = find_virtual_method_in_class($VirtFuncName, $BaseClass_Id, $LibVersion);
        if($VirtMethodInClass)
        {
            return $VirtMethodInClass;
        }
        my $VirtMethodInBaseClasses = find_virtual_method_in_base_classes($VirtFuncName, $BaseClass_Id, $LibVersion);
        if($VirtMethodInBaseClasses)
        {
            return $VirtMethodInBaseClasses;
        }
    }
    return "";
}

sub find_virtual_method_in_class($$$)
{
    my ($VirtFuncName, $Checked_ClassId, $LibVersion) = @_;
    my $Suffix = getFuncSuffix($VirtFuncName, $LibVersion);
    foreach my $Checked_VirtFuncName (keys(%{$ClassIdVirtFunc{$LibVersion}{$Checked_ClassId}}))
    {
        if($Suffix eq getFuncSuffix($Checked_VirtFuncName, $LibVersion)
            and ((not $CompleteSignature{$LibVersion}{$VirtFuncName}{"Constructor"} and not $CompleteSignature{$LibVersion}{$VirtFuncName}{"Destructor"} and $CompleteSignature{$LibVersion}{$VirtFuncName}{"ShortName"} eq $CompleteSignature{$LibVersion}{$Checked_VirtFuncName}{"ShortName"}) or ($CompleteSignature{$LibVersion}{$VirtFuncName}{"Constructor"} or $CompleteSignature{$LibVersion}{$VirtFuncName}{"Destructor"})))
        {
            return $Checked_VirtFuncName;
        }
    }
    return "";
}

sub getFuncSuffix($$)
{
    my ($FuncName, $LibVersion) = @_;
    my $ClassId = $CompleteSignature{$LibVersion}{$FuncName}{"Class"};
    my $ClassName = $TypeDescr{$LibVersion}{$Tid_TDid{$LibVersion}{$ClassId}}{$ClassId}{"Name"};
    my $ShortName = $CompleteSignature{$LibVersion}{$FuncName}{"ShortName"};
    my $Suffix = $tr_name{$FuncName};
    $Suffix =~ s/\A$ClassName\:\:[~]*$ShortName[ ]*//g;
    return $Suffix;
}

sub isRecurType($$$$)
{
	foreach (@RecurTypes)
	{
		if($_->{"Tid1"} eq $_[0]
            and $_->{"TDid1"} eq $_[1]
            and $_->{"Tid2"} eq $_[2]
            and $_->{"TDid2"} eq $_[3])
		{
			return 1;
		}
	}
	return 0;
}

sub find_MemberPair_Pos_byName($$)
{
    my ($Member_Name, $Pair_Type) = @_;
    foreach my $MemberPair_Pos (sort keys(%{$Pair_Type->{"Memb"}}))
    {
        if($Pair_Type->{"Memb"}{$MemberPair_Pos}{"name"} eq $Member_Name)
        {
            return $MemberPair_Pos;
        }
    }
    return "lost";
}

sub getBitfieldSum($$)
{
    my ($Member_Pos, $Pair_Type) = @_;
    my $BitfieldSum = 0;
    while($Member_Pos>-1)
    {
        return $BitfieldSum if(not $Pair_Type->{"Memb"}{$Member_Pos}{"bitfield"});
        $BitfieldSum += $Pair_Type->{"Memb"}{$Member_Pos}{"bitfield"};
        $Member_Pos -= 1;
    }
    return $BitfieldSum;
}

sub find_MemberPair_Pos_byVal($$)
{
    my ($Member_Value, $Pair_Type) = @_;
    foreach my $MemberPair_Pos (sort keys(%{$Pair_Type->{"Memb"}}))
    {
        if($Pair_Type->{"Memb"}{$MemberPair_Pos}{"value"} eq $Member_Value)
        {
            return $MemberPair_Pos;
        }
    }
    return "lost";
}

my %Priority_Value=(
"High"=>3,
"Medium"=>2,
"Low"=>1
);

sub max_priority($$)
{
    my ($Priority1, $Priority2) = @_;
    if(cmp_priority($Priority1, $Priority2))
    {
        return $Priority1;
    }
    else
    {
        return $Priority2;
    }
}

sub cmp_priority($$)
{
    my ($Priority1, $Priority2) = @_;
    return ($Priority_Value{$Priority1}>$Priority_Value{$Priority2});
}

sub set_Problems_Priority()
{
    foreach my $InterfaceName (sort keys(%CompatProblems))
    {
        foreach my $Kind (sort keys(%{$CompatProblems{$InterfaceName}}))
        {
            foreach my $Location (sort keys(%{$CompatProblems{$InterfaceName}{$Kind}}))
            {
                my $IsInTypeInternals = $CompatProblems{$InterfaceName}{$Kind}{$Location}{"IsInTypeInternals"};
                my $InitialType_Type = $CompatProblems{$InterfaceName}{$Kind}{$Location}{"InitialType_Type"};
                if($Kind eq "Function_Become_Static")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "High";
                }
                elsif($Kind eq "Function_Become_NonStatic")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "High";
                }
                elsif($Kind eq "Parameter_Type_And_Size")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "High";
                }
                elsif($Kind eq "Parameter_Type")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                }
                elsif($Kind eq "Withdrawn_Parameter")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Medium";
                }
                elsif($Kind eq "Added_Parameter")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Medium";
                }
                elsif($Kind eq "Parameter_BaseType_And_Size")
                {
                    if($InitialType_Type eq "Pointer")
                    {
                        $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                    }
                    else
                    {
                        $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "High";
                    }
                }
                elsif($Kind eq "Parameter_BaseType")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                }
                elsif($Kind eq "Parameter_PointerLevel")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "High";
                }
                elsif($Kind eq "Return_Type_And_Size")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Medium";
                }
                elsif($Kind eq "Return_Type")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                }
                elsif($Kind eq "Return_BaseType_And_Size")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Medium";
                }
                elsif($Kind eq "Return_BaseType")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                }
                elsif($Kind eq "Return_PointerLevel")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                }
                if($Kind eq "Added_Virtual_Function")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "High";
                }
                elsif($Kind eq "Withdrawn_Virtual_Function")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "High";
                }
                elsif($Kind eq "Virtual_Function_Position")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "High";
                }
                elsif($Kind eq "Virtual_Function_Redefinition")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                }
                elsif($Kind eq "Virtual_Function_Redefinition_B")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                }
                elsif($Kind eq "Size")
                {
                    if(($InitialType_Type eq "Pointer") or ($InitialType_Type eq "Array"))
                    {
                        $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                    }
                    else
                    {
                        if($IsInTypeInternals)
                        {
                            $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Medium";
                        }
                        else
                        {
                            $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "High";
                        }
                    }
                }
                elsif($Kind eq "BaseType")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                }
                elsif($Kind eq "Added_Member_And_Size")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                }
                elsif($Kind eq "Added_Middle_Member_And_Size")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Medium";
                }
                elsif($Kind eq "Withdrawn_Member_And_Size")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Medium";
                }
                elsif($Kind eq "Withdrawn_Member")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                }
                elsif($Kind eq "Withdrawn_Middle_Member_And_Size")
                {
                    if($IsInTypeInternals)
                    {
                        $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Medium";
                    }
                    else
                    {
                        $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "High";
                    }
                }
                elsif($Kind eq "Withdrawn_Middle_Member")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Medium";
                }
                elsif($Kind eq "Member_Rename")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                }
                elsif($Kind eq "Enum_Member_Value")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Medium";
                }
                elsif($Kind eq "Enum_Member_Name")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                }
                elsif($Kind eq "Member_Type_And_Size")
                {
                    if(($InitialType_Type eq "Pointer") or ($InitialType_Type eq "Array"))
                    {
                        $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                    }
                    else
                    {
                        if($IsInTypeInternals)
                        {
                            $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                        }
                        else
                        {
                            $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "High";
                        }
                    }
                }
                elsif($Kind eq "Member_Type")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                }
                elsif($Kind eq "Member_BaseType_And_Size")
                {
                    if(($InitialType_Type eq "Pointer") or ($InitialType_Type eq "Array"))
                    {
                        $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                    }
                    else
                    {
                        if($IsInTypeInternals)
                        {
                            $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                        }
                        else
                        {
                            $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "High";
                        }
                    }
                }
                elsif($Kind eq "Member_BaseType")
                {
                    $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                }
                elsif($Kind eq "Member_PointerLevel")
                {
                    if(($InitialType_Type eq "Pointer") or ($InitialType_Type eq "Array"))
                    {
                        $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                    }
                    else
                    {
                        if($IsInTypeInternals)
                        {
                            $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Low";
                        }
                        else
                        {
                            $CompatProblems{$InterfaceName}{$Kind}{$Location}{"Priority"} = "Medium";
                        }
                    }
                }
            }
        }
    }
}

sub pushType($$$$)
{
    my %TypeDescriptor=(
        "Tid1"  => $_[0],
        "TDid1" => $_[1],
        "Tid2"  => $_[2],
        "TDid2" => $_[3]  );
    push(@RecurTypes, \%TypeDescriptor);
}

sub mergeTypes($$$$)
{
    my ($Type1_Id, $Type1_DId, $Type2_Id, $Type2_DId) = @_;
    my (%Sub_SubProblems, %SubProblems) = ();
    if((not $Type1_Id and not $Type1_DId) or (not $Type2_Id and not $Type2_DId))
    {#Not Empty Inputs Only
        return ();
    }
    if($Cache{"mergeTypes"}{$Type1_Id}{$Type1_DId}{$Type2_Id}{$Type2_DId})
    {#Already Merged
        return %{$Cache{"mergeTypes"}{$Type1_Id}{$Type1_DId}{$Type2_Id}{$Type2_DId}};
    }
    my %Type1 = get_Type($Type1_DId, $Type1_Id, 1);
    my %Type2 = get_Type($Type2_DId, $Type2_Id, 2);
	my %Type1_Pure = get_PureType($Type1_DId, $Type1_Id, 1);
    my %Type2_Pure = get_PureType($Type2_DId, $Type2_Id, 2);
    return () if(not $Type1_Pure{"Size"} or not $Type2_Pure{"Size"});
    if(isRecurType($Type1_Pure{"Tid"}, $Type1_Pure{"TDid"}, $Type2_Pure{"Tid"}, $Type2_Pure{"TDid"}))
    {#Recursive Declarations
        return ();
    }
    return if(not $Type1_Pure{"Name"} or not $Type2_Pure{"Name"});
    return if($OpaqueTypes{1}{$Type1_Pure{"Name"}} or $OpaqueTypes{2}{$Type1_Pure{"Name"}} or $OpaqueTypes{1}{$Type1{"Name"}} or $OpaqueTypes{2}{$Type1{"Name"}});
    
    my %Typedef_1 = goToFirst($Type1{"TDid"}, $Type1{"Tid"}, 1, "Typedef");
    my %Typedef_2 = goToFirst($Type2{"TDid"}, $Type2{"Tid"}, 2, "Typedef");
    if($Typedef_1{"Type"} eq "Typedef" and $Typedef_2{"Type"} eq "Typedef" and $Typedef_1{"Name"} eq $Typedef_2{"Name"})
    {
        my %Base_1 = get_OneStep_BaseType($Typedef_1{"TDid"}, $Typedef_1{"Tid"}, 1);
        my %Base_2 = get_OneStep_BaseType($Typedef_2{"TDid"}, $Typedef_2{"Tid"}, 2);
        if($Base_1{"Name"}!~/anon\-/ and $Base_2{"Name"}!~/anon\-/
            and ($Base_1{"Name"} ne $Base_2{"Name"}))
        {
            %{$SubProblems{"BaseType"}{$Typedef_1{"Name"}}}=(
                "Type_Name"=>$Typedef_1{"Name"},
                "Type_Type"=>"Typedef",
                "Header"=>$Typedef_2{"Header"},
                "Line"=>$Typedef_2{"Line"},
                "Old_Value"=>$Base_1{"Name"},
                "New_Value"=>$Base_2{"Name"}  );
        }
    }
    
    if(($Type1_Pure{"Name"} ne $Type2_Pure{"Name"}) and ($Type1_Pure{"Type"} ne "Pointer") and $Type1_Pure{"Name"}!~/anon\-/)
    {#Different types
        return ();
    }
    
	pushType($Type1_Pure{"Tid"}, $Type1_Pure{"TDid"}, $Type2_Pure{"Tid"}, $Type2_Pure{"TDid"});
	if(($Type1_Pure{"Name"} eq $Type2_Pure{"Name"}) and ($Type1_Pure{"Type"} =~ /Struct|Class|Union/))
	{#Check Size
		if($Type1_Pure{"Size"} ne $Type2_Pure{"Size"})
		{
            %{$SubProblems{"Size"}{$Type1_Pure{"Name"}}}=(
                "Type_Name"=>$Type1_Pure{"Name"},
                "Type_Type"=>$Type1_Pure{"Type"},
                "Header"=>$Type2_Pure{"Header"},
                "Line"=>$Type2_Pure{"Line"},
                "Old_Value"=>$Type1_Pure{"Size"},
                "New_Value"=>$Type2_Pure{"Size"}  );
		}
	}
    if($Type1_Pure{"Name"} and $Type2_Pure{"Name"} and ($Type1_Pure{"Name"} ne $Type2_Pure{"Name"}) and ($Type1_Pure{"Name"} !~ /\Avoid[ ]*\*/) and ($Type2_Pure{"Name"} =~ /\Avoid[ ]*\*/))
    {#Check "void *"
        pop(@RecurTypes);
        return ();
    }
	if($Type1_Pure{"BaseType"}{"Tid"} and $Type2_Pure{"BaseType"}{"Tid"})
	{#Check Base Types
		%Sub_SubProblems = &mergeTypes($Type1_Pure{"BaseType"}{"Tid"}, $Type1_Pure{"BaseType"}{"TDid"}, $Type2_Pure{"BaseType"}{"Tid"}, $Type2_Pure{"BaseType"}{"TDid"});
        foreach my $Sub_SubProblemType (keys(%Sub_SubProblems))
        {
            foreach my $Sub_SubLocation (keys(%{$Sub_SubProblems{$Sub_SubProblemType}}))
            {
                %{$SubProblems{$Sub_SubProblemType}{$Sub_SubLocation}} = %{$Sub_SubProblems{$Sub_SubProblemType}{$Sub_SubLocation}};
                $SubProblems{$Sub_SubProblemType}{$Sub_SubLocation}{"InitialType_Type"} = $Type1_Pure{"Type"};
            }
        }
	}
    foreach my $Member_Pos (sort keys(%{$Type1_Pure{"Memb"}}))
    {#Check Members
        next if($Type1_Pure{"Memb"}{$Member_Pos}{"access"} eq "private");
        my $Member_Name = $Type1_Pure{"Memb"}{$Member_Pos}{"name"};
        next if(not $Member_Name);
        my $Member_Location = $Member_Name;
        my $MemberPair_Pos = find_MemberPair_Pos_byName($Member_Name, \%Type2_Pure);
        if(($MemberPair_Pos eq "lost") and (($Type2_Pure{"Type"} eq "Struct") or ($Type2_Pure{"Type"} eq "Class")))
        {#Withdrawn_Member
            if($Member_Pos > keys(%{$Type2_Pure{"Memb"}}) - 1)
            {
                if($Type1_Pure{"Size"} ne $Type2_Pure{"Size"})
                {
                    %{$SubProblems{"Withdrawn_Member_And_Size"}{$Member_Name}}=(
                        "Target"=>$Member_Name,
                        "Type_Name"=>$Type1_Pure{"Name"},
                        "Type_Type"=>$Type1_Pure{"Type"},
                        "Header"=>$Type2_Pure{"Header"},
                        "Line"=>$Type2_Pure{"Line"},
                        "Old_Size"=>$Type1_Pure{"Size"},
                        "New_Size"=>$Type2_Pure{"Size"}  );
                }
                else
                {
                    %{$SubProblems{"Withdrawn_Member"}{$Member_Name}}=(
                        "Target"=>$Member_Name,
                        "Type_Name"=>$Type1_Pure{"Name"},
                        "Type_Type"=>$Type1_Pure{"Type"},
                        "Header"=>$Type2_Pure{"Header"},
                        "Line"=>$Type2_Pure{"Line"}  );
                }
                next;
            }
            elsif($Member_Pos < keys(%{$Type1_Pure{"Memb"}}) - 1)
            {
                my $MemberType_Id = $Type1_Pure{"Memb"}{$Member_Pos}{"type"};
                my %MemberType_Pure = get_PureType($Tid_TDid{1}{$MemberType_Id}, $MemberType_Id, 1);
                my $MemberStraightPairType_Id = $Type2_Pure{"Memb"}{$Member_Pos}{"type"};
                my %MemberStraightPairType_Pure = get_PureType($Tid_TDid{2}{$MemberStraightPairType_Id}, $MemberStraightPairType_Id, 2);
                
                if(($MemberType_Pure{"Size"} eq $MemberStraightPairType_Pure{"Size"}) and find_MemberPair_Pos_byName($Type2_Pure{"Memb"}{$Member_Pos}{"name"}, \%Type1_Pure) eq "lost")
                {
                    %{$SubProblems{"Member_Rename"}{$Member_Name}}=(
                        "Target"=>$Member_Name,
                        "Type_Name"=>$Type1_Pure{"Name"},
                        "Type_Type"=>$Type1_Pure{"Type"},
                        "Header"=>$Type2_Pure{"Header"},
                        "Line"=>$Type2_Pure{"Line"},
                        "Old_Value"=>$Type1_Pure{"Memb"}{$Member_Pos}{"name"},
                        "New_Value"=>$Type2_Pure{"Memb"}{$Member_Pos}{"name"}  );
                    $MemberPair_Pos = $Member_Pos;
                }
                else
                {
                    if($Type1_Pure{"Memb"}{$Member_Pos}{"bitfield"})
                    {
                        my $BitfieldSum = getBitfieldSum($Member_Pos-1, \%Type1_Pure)%($POINTER_SIZE*8);
                        if($BitfieldSum and $BitfieldSum-$Type2_Pure{"Memb"}{$Member_Pos}{"bitfield"}>=0)
                        {
                            %{$SubProblems{"Withdrawn_Middle_Member"}{$Member_Name}}=(
                            "Target"=>$Member_Name,
                            "Type_Name"=>$Type1_Pure{"Name"},
                            "Type_Type"=>$Type1_Pure{"Type"},
                            "Header"=>$Type2_Pure{"Header"},
                            "Line"=>$Type2_Pure{"Line"}  );
                            next;
                        }
                    }
                    %{$SubProblems{"Withdrawn_Middle_Member_And_Size"}{$Member_Name}}=(
                        "Target"=>$Member_Name,
                        "Type_Name"=>$Type1_Pure{"Name"},
                        "Type_Type"=>$Type1_Pure{"Type"},
                        "Header"=>$Type2_Pure{"Header"},
                        "Line"=>$Type2_Pure{"Line"}  );
                    next;
                }
            }
        }
        if($Type1_Pure{"Type"} eq "Enum")
        {
            my $Member_Value1 = $Type1_Pure{"Memb"}{$Member_Pos}{"value"};
            next if(not $Member_Value1);
            my $Member_Value2 = $Type2_Pure{"Memb"}{$MemberPair_Pos}{"value"};
            if($MemberPair_Pos eq "lost")
            {
                $MemberPair_Pos = find_MemberPair_Pos_byVal($Member_Value1, \%Type2_Pure);
                if($MemberPair_Pos ne "lost")
                {
                    %{$SubProblems{"Enum_Member_Name"}{$Type1_Pure{"Memb"}{$Member_Pos}{"value"}}}=(
                        "Target"=>$Type1_Pure{"Memb"}{$Member_Pos}{"value"},
                        "Type_Name"=>$Type1_Pure{"Name"},
                        "Type_Type"=>"Enum",
                        "Header"=>$Type2_Pure{"Header"},
                        "Line"=>$Type2_Pure{"Line"},
                        "Old_Value"=>$Type1_Pure{"Memb"}{$Member_Pos}{"name"},
                        "New_Value"=>$Type2_Pure{"Memb"}{$MemberPair_Pos}{"name"}  );
                }
            }
            else
            {
                if($Member_Value1 ne "" and $Member_Value2 ne "")
                {
                    if($Member_Value1 ne $Member_Value2)
                    {
                        %{$SubProblems{"Enum_Member_Value"}{$Member_Name}}=(
                            "Target"=>$Member_Name,
                            "Type_Name"=>$Type1_Pure{"Name"},
                            "Type_Type"=>"Enum",
                            "Header"=>$Type2_Pure{"Header"},
                            "Line"=>$Type2_Pure{"Line"},
                            "Old_Value"=>$Type1_Pure{"Memb"}{$Member_Pos}{"value"},
                            "New_Value"=>$Type2_Pure{"Memb"}{$MemberPair_Pos}{"value"}  );
                    }
                }
            }
            next;
        }
        my $MemberType1_Id = $Type1_Pure{"Memb"}{$Member_Pos}{"type"};
        my $MemberType2_Id = $Type2_Pure{"Memb"}{$MemberPair_Pos}{"type"};
        %Sub_SubProblems = detectTypeChange($MemberType1_Id, $MemberType2_Id, "Member");
        foreach my $Sub_SubProblemType (keys(%Sub_SubProblems))
        {
            %{$SubProblems{$Sub_SubProblemType}{$Member_Name}}=(
                "Target"=>$Member_Name,
                "Member_Position"=>$Member_Pos,
                "Type_Name"=>$Type1_Pure{"Name"},
                "Type_Type"=>$Type1_Pure{"Type"},
                "Header"=>$Type2_Pure{"Header"},
                "Line"=>$Type2_Pure{"Line"});
            @{$SubProblems{$Sub_SubProblemType}{$Member_Name}}{keys(%{$Sub_SubProblems{$Sub_SubProblemType}})} = values %{$Sub_SubProblems{$Sub_SubProblemType}};
        }
        if($MemberType1_Id and $MemberType2_Id)
        {#checking member type change (replace)
            %Sub_SubProblems = &mergeTypes($MemberType1_Id, $Tid_TDid{1}{$MemberType1_Id}, $MemberType2_Id, $Tid_TDid{2}{$MemberType2_Id});
            foreach my $Sub_SubProblemType (keys(%Sub_SubProblems))
            {
                foreach my $Sub_SubLocation (keys(%{$Sub_SubProblems{$Sub_SubProblemType}}))
                {
                    my $NewLocation = ($Sub_SubLocation)?$Member_Location."->".$Sub_SubLocation:$Member_Location;
                    %{$SubProblems{$Sub_SubProblemType}{$NewLocation}}=(
                        "IsInTypeInternals"=>"Yes");
                    @{$SubProblems{$Sub_SubProblemType}{$NewLocation}}{keys(%{$Sub_SubProblems{$Sub_SubProblemType}{$Sub_SubLocation}})} = values %{$Sub_SubProblems{$Sub_SubProblemType}{$Sub_SubLocation}};
                    if($Sub_SubLocation !~ /\-\>/)
                    {
                        $SubProblems{$Sub_SubProblemType}{$NewLocation}{"Member_Type_Name"} = get_TypeName($MemberType1_Id, 1);
                        $SubProblems{$Sub_SubProblemType}{$NewLocation}{"Start_Type_Name"} = get_TypeName($MemberType1_Id, 1);
                    }
                }
            }
        }
    }
    if(($Type2_Pure{"Type"} eq "Struct") or ($Type2_Pure{"Type"} eq "Class"))
    {
        foreach my $Member_Pos (sort keys(%{$Type2_Pure{"Memb"}}))
        {#checking added members
            next if(not $Type2_Pure{"Memb"}{$Member_Pos}{"name"});
            my $MemberPair_Pos = find_MemberPair_Pos_byName($Type2_Pure{"Memb"}{$Member_Pos}{"name"}, \%Type1_Pure);
            if($MemberPair_Pos eq "lost")
            {#Added_Member
                if($Member_Pos > keys(%{$Type1_Pure{"Memb"}}) - 1)
                {
                    if($Type1_Pure{"Size"} ne $Type2_Pure{"Size"})
                    {
                        if($Type2_Pure{"Memb"}{$Member_Pos}{"bitfield"})
                        {
                            my $BitfieldSum = getBitfieldSum($Member_Pos-1, \%Type2_Pure)%($POINTER_SIZE*8);
                            next if($BitfieldSum and $BitfieldSum<=$POINTER_SIZE*8-$Type2_Pure{"Memb"}{$Member_Pos}{"bitfield"});
                        }
                        %{$SubProblems{"Added_Member_And_Size"}{$Type2_Pure{"Memb"}{$Member_Pos}{"name"}}}=(
                            "Target"=>$Type2_Pure{"Memb"}{$Member_Pos}{"name"},
                            "Type_Name"=>$Type1_Pure{"Name"},
                            "Type_Type"=>$Type1_Pure{"Type"},
                            "Header"=>$Type2_Pure{"Header"},
                            "Line"=>$Type2_Pure{"Line"}  );
                    }
                }
                else
                {
                    my $MemberType_Id = $Type2_Pure{"Memb"}{$Member_Pos}{"type"};
                    my $MemberType_DId = $Tid_TDid{2}{$MemberType_Id};
                    my %MemberType_Pure = get_PureType($MemberType_DId, $MemberType_Id, 2);
                    
                    my $MemberStraightPairType_Id = $Type1_Pure{"Memb"}{$Member_Pos}{"type"};
                    my %MemberStraightPairType_Pure = get_PureType($Tid_TDid{1}{$MemberStraightPairType_Id}, $MemberStraightPairType_Id, 1);
                    
                    if(($MemberType_Pure{"Size"} eq $MemberStraightPairType_Pure{"Size"}) and find_MemberPair_Pos_byName($Type1_Pure{"Memb"}{$Member_Pos}{"name"}, \%Type2_Pure) eq "lost")
                    {
                        next if($Type1_Pure{"Memb"}{$Member_Pos}{"access"} eq "private");
                        %{$SubProblems{"Member_Rename"}{$Type2_Pure{"Memb"}{$Member_Pos}{"name"}}}=(
                            "Target"=>$Type1_Pure{"Memb"}{$Member_Pos}{"name"},
                            "Type_Name"=>$Type1_Pure{"Name"},
                            "Type_Type"=>$Type1_Pure{"Type"},
                            "Header"=>$Type2_Pure{"Header"},
                            "Line"=>$Type2_Pure{"Line"},
                            "Old_Value"=>$Type1_Pure{"Memb"}{$Member_Pos}{"name"},
                            "New_Value"=>$Type2_Pure{"Memb"}{$Member_Pos}{"name"}  );
                    }
                    else
                    {
                        if($Type1_Pure{"Size"} ne $Type2_Pure{"Size"})
                        {
                            if($Type2_Pure{"Memb"}{$Member_Pos}{"bitfield"})
                            {
                                my $BitfieldSum = getBitfieldSum($Member_Pos-1, \%Type2_Pure)%($POINTER_SIZE*8);
                                next if($BitfieldSum and $BitfieldSum<=$POINTER_SIZE*8-$Type2_Pure{"Memb"}{$Member_Pos}{"bitfield"});
                            }
                            %{$SubProblems{"Added_Middle_Member_And_Size"}{$Type2_Pure{"Memb"}{$Member_Pos}{"name"}}}=(
                                "Target"=>$Type2_Pure{"Memb"}{$Member_Pos}{"name"},
                                "Type_Name"=>$Type1_Pure{"Name"},
                                "Type_Type"=>$Type1_Pure{"Type"},
                                "Header"=>$Type2_Pure{"Header"},
                                "Line"=>$Type2_Pure{"Line"}  );
                        }
                    }
                }
            }
        }
    }
    %{$Cache{"mergeTypes"}{$Type1_Id}{$Type1_DId}{$Type2_Id}{$Type2_DId}} = %SubProblems;
	pop(@RecurTypes);
    return %SubProblems;
}

sub get_TypeName($$)
{
    my ($TypeId, $LibVersion) = @_;
    return $TypeDescr{$LibVersion}{$Tid_TDid{$LibVersion}{$TypeId}}{$TypeId}{"Name"};
}

sub goToFirst($$$$)
{
    my ($TypeDId, $TypeId, $LibVersion, $Type_Type) = @_;
    if(defined $Cache{"goToFirst"}{$TypeDId}{$TypeId}{$LibVersion}{$Type_Type})
    {
        return %{$Cache{"goToFirst"}{$TypeDId}{$TypeId}{$LibVersion}{$Type_Type}};
    }
    return () if(not $TypeDescr{$LibVersion}{$TypeDId}{$TypeId});
    my %Type = %{$TypeDescr{$LibVersion}{$TypeDId}{$TypeId}};
    return () if(not $Type{"Type"});
    if($Type{"Type"} ne $Type_Type)
    {
        return () if(not $Type{"BaseType"}{"TDid"} and not $Type{"BaseType"}{"Tid"});
        %Type = goToFirst($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"}, $LibVersion, $Type_Type);
    }
    $Cache{"goToFirst"}{$TypeDId}{$TypeId}{$LibVersion}{$Type_Type} = \%Type;
    return %Type;
}

my %TypeSpecAttributes = (
    "Ref" => 1,
    "Const" => 1,
    "Volatile" => 1,
    "Restrict" => 1,
    "Typedef" => 1
);

sub get_PureType($$$)
{
    my ($TypeDId, $TypeId, $LibVersion) = @_;
    return "" if(not $TypeId);
    if(defined $Cache{"get_PureType"}{$TypeDId}{$TypeId}{$LibVersion})
    {
        return %{$Cache{"get_PureType"}{$TypeDId}{$TypeId}{$LibVersion}};
    }
    return "" if(not $TypeDescr{$LibVersion}{$TypeDId}{$TypeId});
    my %Type = %{$TypeDescr{$LibVersion}{$TypeDId}{$TypeId}};
    if($Type{"Type"} =~ /Struct|Union|Typedef|Class|Enum/)
    {
        $CheckedTypes{$Type{"Name"}} = 1;
    }
    return %Type if(not $Type{"BaseType"}{"TDid"} and not $Type{"BaseType"}{"Tid"});
    if($TypeSpecAttributes{$Type{"Type"}})
    {
        %Type = get_PureType($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"}, $LibVersion);
    }
    $Cache{"get_PureType"}{$TypeDId}{$TypeId}{$LibVersion} = \%Type;
    return %Type;
}

sub get_PointerLevel($$$)
{
    my ($TypeDId, $TypeId, $LibVersion) = @_;
    return "" if(not $TypeId);
    if(defined $Cache{"get_PointerLevel"}{$TypeDId}{$TypeId}{$LibVersion})
    {
        return $Cache{"get_PointerLevel"}{$TypeDId}{$TypeId}{$LibVersion};
    }
    return "" if(not $TypeDescr{$LibVersion}{$TypeDId}{$TypeId});
    my %Type = %{$TypeDescr{$LibVersion}{$TypeDId}{$TypeId}};
    return 0 if(not $Type{"BaseType"}{"TDid"} and not $Type{"BaseType"}{"Tid"});
    my $PointerLevel = 0;
    if($Type{"Type"} eq "Pointer")
    {
        $PointerLevel += 1;
    }
    $PointerLevel += get_PointerLevel($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"}, $LibVersion);
    $Cache{"get_PointerLevel"}{$TypeDId}{$TypeId}{$LibVersion} = $PointerLevel;
    return $PointerLevel;
}

sub get_BaseType($$$)
{
    my ($TypeDId, $TypeId, $LibVersion) = @_;
    return "" if(not $TypeId);
    if(defined $Cache{"get_BaseType"}{$TypeDId}{$TypeId}{$LibVersion})
    {
        return %{$Cache{"get_BaseType"}{$TypeDId}{$TypeId}{$LibVersion}};
    }
    return "" if(not $TypeDescr{$LibVersion}{$TypeDId}{$TypeId});
    my %Type = %{$TypeDescr{$LibVersion}{$TypeDId}{$TypeId}};
    if($Type{"Type"} =~ /Struct|Union|Typedef|Class|Enum/)
    {
        $CheckedTypes{$Type{"Name"}} = 1;
    }
    return %Type if(not $Type{"BaseType"}{"TDid"} and not $Type{"BaseType"}{"Tid"});
    %Type = get_BaseType($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"}, $LibVersion);
    $Cache{"get_BaseType"}{$TypeDId}{$TypeId}{$LibVersion} = \%Type;
    return %Type;
}

sub get_OneStep_BaseType($$$)
{
    my ($TypeDId, $TypeId, $LibVersion) = @_;
    return "" if(not $TypeId);
    return "" if(not $TypeDescr{$LibVersion}{$TypeDId}{$TypeId});
    my %Type = %{$TypeDescr{$LibVersion}{$TypeDId}{$TypeId}};
    return %Type if(not $Type{"BaseType"}{"TDid"} and not $Type{"BaseType"}{"Tid"});
    return get_Type($Type{"BaseType"}{"TDid"}, $Type{"BaseType"}{"Tid"}, $LibVersion);
}

sub get_Type($$$)
{
    my ($TypeDId, $TypeId, $LibVersion) = @_;
    return "" if(not $TypeId);
    return "" if(not $TypeDescr{$LibVersion}{$TypeDId}{$TypeId});
    return %{$TypeDescr{$LibVersion}{$TypeDId}{$TypeId}};
}

sub mergeLibs()
{
    foreach my $Interface (sort keys(%AddedInt))
    {#checking added interfaces
        next if($InternalInterfaces{1}{$Interface} or $InternalInterfaces{2}{$Interface});
        next if(defined $InterfacesListPath and not $InterfacesList{$Interface});
        next if(defined $AppPath and not $InterfacesList_App{$Interface});
        next if($FuncAttr{2}{$Interface}{"Private"});
        next if(not $FuncAttr{2}{$Interface}{"Header"});
        %{$CompatProblems{$Interface}{"Added_Interface"}{"SharedLibrary"}}=(
            "Header"=>$FuncAttr{2}{$Interface}{"Header"},
            "Line"=>$FuncAttr{2}{$Interface}{"Line"},
            "Signature"=>$FuncAttr{2}{$Interface}{"Signature"},
            "New_SoName"=>$Interface_Library{2}{$Interface}  );
    }
    foreach my $Interface (sort keys(%WithdrawnInt))
    {#checking withdrawn interfaces
        next if($InternalInterfaces{1}{$Interface} or $InternalInterfaces{2}{$Interface});
        next if(defined $InterfacesListPath and not $InterfacesList{$Interface});
        next if(defined $AppPath and not $InterfacesList_App{$Interface});
        next if($FuncAttr{1}{$Interface}{"Private"});
        next if(not $FuncAttr{1}{$Interface}{"Header"});
        %{$CompatProblems{$Interface}{"Withdrawn_Interface"}{"SharedLibrary"}}=(
            "Header"=>$FuncAttr{1}{$Interface}{"Header"},
            "Line"=>$FuncAttr{1}{$Interface}{"Line"},
            "Signature"=>$FuncAttr{1}{$Interface}{"Signature"},
            "Old_SoName"=>$Interface_Library{1}{$Interface}  );
    }
}

sub mergeSignatures()
{
    my %SubProblems = ();
    
	prepareInterfaces(1);
	prepareInterfaces(2);
    %FuncDescr=();
    
    initializeClassVirtFunc(1);
    initializeClassVirtFunc(2);
    
    checkVirtFuncRedefinitions(1);
    checkVirtFuncRedefinitions(2);
    
    setVirtFuncPositions(1);
    setVirtFuncPositions(2);
    
    foreach my $Interface (sort keys(%AddedInt))
    {#collecting the attributes of added interfaces
        next if($CheckedInterfaces{$Interface});
        if($CompleteSignature{2}{$Interface})
        {
            if($CompleteSignature{2}{$Interface}{"Private"})
            {
                $FuncAttr{2}{$Interface}{"Private"} = 1;
            }
            if($CompleteSignature{2}{$Interface}{"Protected"})
            {
                $FuncAttr{2}{$Interface}{"Protected"} = 1;
            }
            if($CompleteSignature{2}{$Interface}{"Header"})
            {
                $FuncAttr{2}{$Interface}{"Header"} = $CompleteSignature{2}{$Interface}{"Header"};
            }
            if($CompleteSignature{2}{$Interface}{"Line"})
            {
                $FuncAttr{2}{$Interface}{"Line"} = $CompleteSignature{2}{$Interface}{"Line"};
            }
            $FuncAttr{2}{$Interface}{"Signature"} = get_Signature($Interface, 2);
            foreach my $ParamPos (keys(%{$CompleteSignature{2}{$Interface}{"Param"}}))
            {
                my $ParamType_Id = $CompleteSignature{2}{$Interface}{"Param"}{$ParamPos}{"type"};
                my $ParamType_DId = $Tid_TDid{2}{$ParamType_Id};
                my %ParamType = get_Type($ParamType_DId, $ParamType_Id, 2);
            }
            #checking virtual table
            check_VirtualTable($Interface, 2);
            $CheckedInterfaces{$Interface} = 1;
        }
    }
    foreach my $Interface (sort keys(%WithdrawnInt))
    {#collecting the attributes of withdrawn interfaces
        next if($CheckedInterfaces{$Interface});
        if($CompleteSignature{1}{$Interface})
        {
            if($CompleteSignature{1}{$Interface}{"Private"})
            {
                $FuncAttr{1}{$Interface}{"Private"} = 1;
            }
            if($CompleteSignature{1}{$Interface}{"Protected"})
            {
                $FuncAttr{1}{$Interface}{"Protected"} = 1;
            }
            if($CompleteSignature{1}{$Interface}{"Header"})
            {
                $FuncAttr{1}{$Interface}{"Header"} = $CompleteSignature{1}{$Interface}{"Header"};
            }
            if($CompleteSignature{1}{$Interface}{"Line"})
            {
                $FuncAttr{1}{$Interface}{"Line"} = $CompleteSignature{1}{$Interface}{"Line"};
            }
            $FuncAttr{1}{$Interface}{"Signature"} = get_Signature($Interface, 1);
            foreach my $ParamPos (keys(%{$CompleteSignature{1}{$Interface}{"Param"}}))
            {
                my $ParamType_Id = $CompleteSignature{1}{$Interface}{"Param"}{$ParamPos}{"type"};
                my $ParamType_DId = $Tid_TDid{1}{$ParamType_Id};
                my %ParamType = get_Type($ParamType_DId, $ParamType_Id, 1);
            }
            #checking virtual table
            check_VirtualTable($Interface, 1);
            $CheckedInterfaces{$Interface} = 1;
        }
    }
    
	foreach my $Interface (sort keys(%{$CompleteSignature{1}}))
	{#checking interfaces
        if(($Interface !~ /\@/) and ($SymVer{1}{$Interface} =~ /\A(.*)[\@]+/))
        {
            next if($1 eq $Interface);
        }
        my ($MnglName, $SymbolVersion) = ($Interface, "");
        if($Interface =~ /\A([^@]+)[\@]+([^@]+)\Z/)
        {
            ($MnglName, $SymbolVersion) = ($1, $2);
        }
        next if($InternalInterfaces{1}{$Interface} or $InternalInterfaces{2}{$Interface});
        next if(defined $InterfacesListPath and not $InterfacesList{$Interface});
        next if(defined $AppPath and not $InterfacesList_App{$Interface});
        next if($CheckedInterfaces{$Interface});
        next if($CompleteSignature{1}{$Interface}{"Private"});
        next if(not $CompleteSignature{1}{$Interface}{"Header"} or not $CompleteSignature{2}{$Interface}{"Header"});
		next if(not $CompleteSignature{1}{$Interface}{"MnglName"} or not $CompleteSignature{2}{$Interface}{"MnglName"});
        next if((not $CompleteSignature{1}{$Interface}{"PureVirt"} and $CompleteSignature{2}{$Interface}{"PureVirt"}) or ($CompleteSignature{1}{$Interface}{"PureVirt"} and not $CompleteSignature{2}{$Interface}{"PureVirt"}));
        $CheckedInterfaces{$Interface} = 1;
        #checking virtual table
        check_VirtualTable($Interface, 1);
        #checking attributes
		if($CompleteSignature{2}{$Interface}{"Static"} and not $CompleteSignature{1}{$Interface}{"Static"})
		{
            %{$CompatProblems{$Interface}{"Function_Become_Static"}{"Attributes"}}=(
                "Header"=>$CompleteSignature{1}{$Interface}{"Header"},
                "Line"=>$CompleteSignature{1}{$Interface}{"Line"},
                "Signature"=>get_Signature($Interface, 1),
                "Old_SoName"=>$Interface_Library{1}{$Interface},
                "New_SoName"=>$Interface_Library{2}{$Interface}  );
		}
		elsif(not $CompleteSignature{2}{$Interface}{"Static"} and $CompleteSignature{1}{$Interface}{"Static"})
		{
            %{$CompatProblems{$Interface}{"Function_Become_NonStatic"}{"Attributes"}}=(
                "Header"=>$CompleteSignature{1}{$Interface}{"Header"},
                "Line"=>$CompleteSignature{1}{$Interface}{"Line"},
                "Signature"=>get_Signature($Interface, 1),
                "Old_SoName"=>$Interface_Library{1}{$Interface},
                "New_SoName"=>$Interface_Library{2}{$Interface}  );
		}
        if($CompleteSignature{1}{$Interface}{"Virt"} and $CompleteSignature{2}{$Interface}{"Virt"})
        {
            if($CompleteSignature{1}{$Interface}{"Position"}!=$CompleteSignature{2}{$Interface}{"Position"})
            {
                my $Class_Id = $CompleteSignature{1}{$Interface}{"Class"};
                my $Class_DId = $Tid_TDid{1}{$Class_Id};
                my %Class_Type = get_Type($Class_DId, $Class_Id, 1);
                %{$CompatProblems{$Interface}{"Virtual_Function_Position"}{$tr_name{$MnglName}}}=(
                "Type_Name"=>$Class_Type{"Name"},
                "Type_Type"=>$Class_Type{"Type"},
                "Header"=>$Class_Type{"Header"},
                "Line"=>$Class_Type{"Line"},
                "Old_Value"=>$CompleteSignature{1}{$Interface}{"Position"},
                "New_Value"=>$CompleteSignature{2}{$Interface}{"Position"},
                "Signature"=>get_Signature($Interface, 1),
                "Target"=>$tr_name{$MnglName},
                "Old_SoName"=>$Interface_Library{1}{$Interface},
                "New_SoName"=>$Interface_Library{2}{$Interface}  );
            }
        }
        foreach my $ParamPos (sort {int($a) <=> int($b)} keys(%{$CompleteSignature{2}{$Interface}{"Param"}}))
        {#checking added parameters
            last if($Interface=~/\A_Z/);
            if(not defined $CompleteSignature{1}{$Interface}{"Param"}{$ParamPos})
            {#checking withdrawn parameters
                my $ParamType2_Id = $CompleteSignature{2}{$Interface}{"Param"}{$ParamPos}{"type"};
                my $Parameter_Name = $CompleteSignature{2}{$Interface}{"Param"}{$ParamPos}{"name"};
                last if(get_TypeName($ParamType2_Id, 2) eq "...");
                %{$CompatProblems{$Interface}{"Added_Parameter"}{num_to_str($ParamPos+1)." Parameter"}}=(
                    "Target"=>$Parameter_Name,
                    "Parameter_Position"=>$ParamPos,
                    "Signature"=>get_Signature($Interface, 1),
                    "Header"=>$CompleteSignature{1}{$Interface}{"Header"},
                    "Line"=>$CompleteSignature{1}{$Interface}{"Line"},
                    "Old_SoName"=>$Interface_Library{1}{$Interface},
                    "New_SoName"=>$Interface_Library{2}{$Interface});
            }
        }
		foreach my $ParamPos (sort {int($a) <=> int($b)} keys(%{$CompleteSignature{1}{$Interface}{"Param"}}))
		{#checking parameters
            my $ParamType1_Id = $CompleteSignature{1}{$Interface}{"Param"}{$ParamPos}{"type"};
            my $Parameter_Name = $CompleteSignature{1}{$Interface}{"Param"}{$ParamPos}{"name"};
            if(not defined $CompleteSignature{2}{$Interface}{"Param"}{$ParamPos} and get_TypeName($ParamType1_Id, 1) ne "..." and $Interface!~/\A_Z/)
            {#checking withdrawn parameters
                %{$CompatProblems{$Interface}{"Withdrawn_Parameter"}{num_to_str($ParamPos+1)." Parameter"}}=(
                    "Target"=>$Parameter_Name,
                    "Parameter_Position"=>$ParamPos,
                    "Signature"=>get_Signature($Interface, 1),
                    "Header"=>$CompleteSignature{1}{$Interface}{"Header"},
                    "Line"=>$CompleteSignature{1}{$Interface}{"Line"},
                    "Old_SoName"=>$Interface_Library{1}{$Interface},
                    "New_SoName"=>$Interface_Library{2}{$Interface});
                next;
            }
			my $ParamType2_Id = $CompleteSignature{2}{$Interface}{"Param"}{$ParamPos}{"type"};
            next if(not $ParamType1_Id or not $ParamType2_Id);
            my $Parameter_Location = ($Parameter_Name)?$Parameter_Name:num_to_str($ParamPos+1)." Parameter";
            
            #checking type change(replace)
            %SubProblems = detectTypeChange($ParamType1_Id, $ParamType2_Id, "Parameter");
            foreach my $SubProblemType (keys(%SubProblems))
            {
                %{$CompatProblems{$Interface}{$SubProblemType}{$Parameter_Location}}=(
                    "Target"=>$Parameter_Name,
                    "Parameter_Position"=>$ParamPos,
                    "Signature"=>get_Signature($Interface, 1),
                    "Header"=>$CompleteSignature{1}{$Interface}{"Header"},
                    "Line"=>$CompleteSignature{1}{$Interface}{"Line"},
                    "Old_SoName"=>$Interface_Library{1}{$Interface},
                    "New_SoName"=>$Interface_Library{2}{$Interface});
                @{$CompatProblems{$Interface}{$SubProblemType}{$Parameter_Location}}{keys(%{$SubProblems{$SubProblemType}})} = values %{$SubProblems{$SubProblemType}};
            }
            @RecurTypes = ();
            #checking type definition changes
            %SubProblems = mergeTypes($ParamType1_Id, $Tid_TDid{1}{$ParamType1_Id}, $ParamType2_Id, $Tid_TDid{2}{$ParamType2_Id});
            foreach my $SubProblemType (keys(%SubProblems))
            {
                foreach my $SubLocation (keys(%{$SubProblems{$SubProblemType}}))
                {
                    my $NewLocation = ($SubLocation)?$Parameter_Location."->".$SubLocation:$Parameter_Location;
                    %{$CompatProblems{$Interface}{$SubProblemType}{$NewLocation}}=(
                        "Signature"=>get_Signature($Interface, 1),
                        "Header"=>$CompleteSignature{1}{$Interface}{"Header"},
                        "Line"=>$CompleteSignature{1}{$Interface}{"Line"},
                        "Old_SoName"=>$Interface_Library{1}{$Interface},
                        "New_SoName"=>$Interface_Library{2}{$Interface},
                        "Parameter_Type_Name"=>get_TypeName($ParamType1_Id, 1),
                        "Parameter_Position"=>$ParamPos,
                        "Parameter_Name"=>$Parameter_Name);
                    @{$CompatProblems{$Interface}{$SubProblemType}{$NewLocation}}{keys(%{$SubProblems{$SubProblemType}{$SubLocation}})} = values %{$SubProblems{$SubProblemType}{$SubLocation}};
                    if($SubLocation !~ /\-\>/)
                    {
                        $CompatProblems{$Interface}{$SubProblemType}{$NewLocation}{"Start_Type_Name"} = get_TypeName($ParamType1_Id, 1);
                    }
                }
            }
		}
		#checking return type
		my $ReturnType1_Id = $CompleteSignature{1}{$Interface}{"Return"};
		my $ReturnType2_Id = $CompleteSignature{2}{$Interface}{"Return"};
        %SubProblems = detectTypeChange($ReturnType1_Id, $ReturnType2_Id, "Return");
        foreach my $SubProblemType (keys(%SubProblems))
        {
            %{$CompatProblems{$Interface}{$SubProblemType}{"RetVal"}}=(
                "Signature"=>get_Signature($Interface, 1),
                "Header"=>$CompleteSignature{1}{$Interface}{"Header"},
                "Line"=>$CompleteSignature{1}{$Interface}{"Line"},
                "Old_SoName"=>$Interface_Library{1}{$Interface},
                "New_SoName"=>$Interface_Library{2}{$Interface});
            @{$CompatProblems{$Interface}{$SubProblemType}{"RetVal"}}{keys(%{$SubProblems{$SubProblemType}})} = values %{$SubProblems{$SubProblemType}};
        }
        if($ReturnType1_Id and $ReturnType2_Id)
        {
            @RecurTypes = ();
            %SubProblems = mergeTypes($ReturnType1_Id, $Tid_TDid{1}{$ReturnType1_Id}, $ReturnType2_Id, $Tid_TDid{2}{$ReturnType2_Id});
            foreach my $SubProblemType (keys(%SubProblems))
            {
                foreach my $SubLocation (keys(%{$SubProblems{$SubProblemType}}))
                {
                    my $NewLocation = ($SubLocation)?"RetVal->".$SubLocation:"RetVal";
                    %{$CompatProblems{$Interface}{$SubProblemType}{$NewLocation}}=(
                        "Signature"=>get_Signature($Interface, 1),
                        "Header"=>$CompleteSignature{1}{$Interface}{"Header"},
                        "Line"=>$CompleteSignature{1}{$Interface}{"Line"},
                        "Old_SoName"=>$Interface_Library{1}{$Interface},
                        "New_SoName"=>$Interface_Library{2}{$Interface},
                        "Return_Type_Name"=>get_TypeName($ReturnType1_Id, 1) );
                    @{$CompatProblems{$Interface}{$SubProblemType}{$NewLocation}}{keys(%{$SubProblems{$SubProblemType}{$SubLocation}})} = values %{$SubProblems{$SubProblemType}{$SubLocation}};
                    if($SubLocation !~ /\-\>/)
                    {
                        $CompatProblems{$Interface}{$SubProblemType}{$NewLocation}{"Start_Type_Name"} = get_TypeName($ReturnType1_Id, 1);
                    }
                }
            }
        }
        
		#checking object type
		my $ObjectType1_Id = $CompleteSignature{1}{$Interface}{"Class"};
		my $ObjectType2_Id = $CompleteSignature{2}{$Interface}{"Class"};
        if($ObjectType1_Id and $ObjectType2_Id)
        {
            my $ThisType1_Id = getTypeIdByName(get_TypeName($ObjectType1_Id, 1)."*const", 1);
            my $ThisType2_Id = getTypeIdByName(get_TypeName($ObjectType2_Id, 2)."*const", 2);
            if($ThisType1_Id and $ThisType2_Id)
            {
                @RecurTypes = ();
		        %SubProblems = mergeTypes($ThisType1_Id, $Tid_TDid{1}{$ThisType1_Id}, $ThisType2_Id, $Tid_TDid{2}{$ThisType2_Id});
                foreach my $SubProblemType (keys(%SubProblems))
                {
                    foreach my $SubLocation (keys(%{$SubProblems{$SubProblemType}}))
                    {
                        my $NewLocation = ($SubLocation)?"Obj->".$SubLocation:"Obj";
                        %{$CompatProblems{$Interface}{$SubProblemType}{$NewLocation}}=(
                            "Signature"=>get_Signature($Interface, 1),
                            "Header"=>$CompleteSignature{1}{$Interface}{"Header"},
                            "Line"=>$CompleteSignature{1}{$Interface}{"Line"},
                            "Old_SoName"=>$Interface_Library{1}{$Interface},
                            "New_SoName"=>$Interface_Library{2}{$Interface},
                            "Object_Type_Name"=>get_TypeName($ObjectType1_Id, 1) );
                        @{$CompatProblems{$Interface}{$SubProblemType}{$NewLocation}}{keys(%{$SubProblems{$SubProblemType}{$SubLocation}})} = values %{$SubProblems{$SubProblemType}{$SubLocation}};
                        if($SubLocation !~ /\-\>/)
                        {
                            $CompatProblems{$Interface}{$SubProblemType}{$NewLocation}{"Start_Type_Name"} = get_TypeName($ObjectType1_Id, 1);
                        }
                    }
                }
            }
        }
	}
    set_Problems_Priority();
}

sub getTypeIdByName($$)
{
    my ($TypeName, $Version) = @_;
    return $TName_Tid{$Version}{correctName($TypeName)};
}

sub detectTypeChange($$$)
{
    my ($Type1_Id, $Type2_Id, $Prefix) = @_;
    my %LocalProblems = ();
    my $Type1_DId = $Tid_TDid{1}{$Type1_Id};
    my $Type2_DId = $Tid_TDid{2}{$Type2_Id};
    my %Type1 = get_Type($Type1_DId, $Type1_Id, 1);
    my %Type2 = get_Type($Type2_DId, $Type2_Id, 2);
    my %Type1_Base = get_BaseType($Type1_DId, $Type1_Id, 1);
    my %Type2_Base = get_BaseType($Type2_DId, $Type2_Id, 2);
    my %Type1_Pure = get_PureType($Type1_DId, $Type1_Id, 1);
    my %Type2_Pure = get_PureType($Type2_DId, $Type2_Id, 2);
    my $Type1_PointerLevel = get_PointerLevel($Type1_DId, $Type1_Id, 1);
    my $Type2_PointerLevel = get_PointerLevel($Type2_DId, $Type2_Id, 2);
    return () if(not $Type1{"Name"} or not $Type2{"Name"} or not $Type1{"Size"} or not $Type2{"Size"} or not $Type1_Pure{"Size"} or not $Type2_Pure{"Size"} or not $Type1_Base{"Name"} or not $Type2_Base{"Name"} or not $Type1_Base{"Size"} or not $Type2_Base{"Size"} or $Type1_PointerLevel eq "" or $Type2_PointerLevel eq "");
    if($Type1_Base{"Name"} ne $Type2_Base{"Name"})
    {#base type change
        if($Type1_Base{"Name"}!~/anon\-/ and $Type2_Base{"Name"}!~/anon\-/)
        {
            if($Type1_Base{"Size"}!=$Type2_Base{"Size"})
            {
                %{$LocalProblems{$Prefix."_BaseType_And_Size"}}=(
                    "Old_Value"=>$Type1_Base{"Name"},
                    "New_Value"=>$Type2_Base{"Name"},
                    "Old_Size"=>$Type1_Base{"Size"},
                    "New_Size"=>$Type2_Base{"Size"},
                    "InitialType_Type"=>$Type1_Pure{"Type"});
            }
            else
            {
                %{$LocalProblems{$Prefix."_BaseType"}}=(
                    "Old_Value"=>$Type1_Base{"Name"},
                    "New_Value"=>$Type2_Base{"Name"},
                    "InitialType_Type"=>$Type1_Pure{"Type"});
            }
        }
    }
    elsif($Type1{"Name"} ne $Type2{"Name"})
    {#type change
        if($Type1{"Name"}!~/anon\-/ and $Type2{"Name"}!~/anon\-/)
        {
            if($Type1{"Size"}!=$Type2{"Size"})
            {
                %{$LocalProblems{$Prefix."_Type_And_Size"}}=(
                    "Old_Value"=>$Type1{"Name"},
                    "New_Value"=>$Type2{"Name"},
                    "Old_Size"=>($Type1{"Type"} eq "Array")?$Type1{"Size"}*$Type1_Base{"Size"}:$Type1{"Size"},
                    "New_Size"=>($Type2{"Type"} eq "Array")?$Type2{"Size"}*$Type2_Base{"Size"}:$Type2{"Size"},
                    "InitialType_Type"=>$Type1_Pure{"Type"});
            }
            else
            {
                %{$LocalProblems{$Prefix."_Type"}}=(
                    "Old_Value"=>$Type1{"Name"},
                    "New_Value"=>$Type2{"Name"},
                    "InitialType_Type"=>$Type1_Pure{"Type"});
            }
        }
    }
    
    if($Type1_PointerLevel!=$Type2_PointerLevel)
    {
        %{$LocalProblems{$Prefix."_PointerLevel"}}=(
            "Old_Value"=>$Type1_PointerLevel,
            "New_Value"=>$Type2_PointerLevel);
    }
    return %LocalProblems;
}

sub htmlSpecChars($)
{
    my $Str = $_[0];
    $Str =~ s/\&([^#])/&amp;$1/g;
    $Str =~ s/</&lt;/g;
    $Str =~ s/>/&gt;/g;
    $Str =~ s/([^ ])( )([^ ])/$1\@ALONE_SP\@$3/g;
    $Str =~ s/ /&nbsp;/g;
    $Str =~ s/\@ALONE_SP\@/ /g;
    $Str =~ s/\n/<br\/>/g;
    return $Str;
}

sub highLight_Signature($)
{
    my $Signature = $_[0];
    return highLight_Signature_PPos_Italic($Signature, "", 0, 0);
}

sub highLight_Signature_Italic($)
{
    my $Signature = $_[0];
    return highLight_Signature_PPos_Italic($Signature, "", 1, 0);
}

sub highLight_Signature_Italic_Color($)
{
    my $Signature = $_[0];
    return highLight_Signature_PPos_Italic($Signature, "", 1, 1);
}

sub highLight_Signature_PPos_Italic($$$$)
{
    my ($FullSignature, $Parameter_Position, $ItalicParams, $ColorParams) = @_;
    my ($Signature, $SymbolVersion) = ($FullSignature, "");
    if($FullSignature =~ /\A(.+)[\@]+(.+)\Z/)
    {
        ($Signature, $SymbolVersion) = ($1, $2);
    }
    if($Signature=~/\Atypeinfo\W/)
    {
        return $Signature.(($SymbolVersion)?"<span class='symver'> \@ $SymbolVersion</span>":"");
    }
    if($Signature!~/\)(| const)\Z/)
    {
        return $Signature.(($SymbolVersion)?"<span class='symver'> \@ $SymbolVersion</span>":"");
    }
    $Signature =~ /(.+?)\(.*\)(| const)\Z/;
    my ($Begin, $End) = ($1, $2);
    my @Parts = ();
    my $Part_Num = 0;
    foreach my $Part (get_Signature_Parts($Signature, 1))
    {
        $Part =~ s/\A\s+|\s+\Z//g;
        my ($Part_Styled, $ParamName) = ($Part, "");
        if($Part=~/\([\*]+(\w+)\)/i)
        {#func-ptr
            $ParamName = $1;
        }
        elsif($Part=~/(\w+)[\,\)]*\Z/i)
        {
            $ParamName = $1;
        }
        if(not $ParamName)
        {
            push(@Parts, $Part);
            next;
        }
        if($ItalicParams and not $TName_Tid{1}{$Part} and not $TName_Tid{2}{$Part})
        {
            if(($Parameter_Position ne "") and ($Part_Num == $Parameter_Position))
            {
                $Part_Styled =~ s!(\W)$ParamName([\,\)]|\Z)!$1<span class='focus_param'>$ParamName</span>$2!ig;
            }
            elsif($ColorParams)
            {
                $Part_Styled =~ s!(\W)$ParamName([\,\)]|\Z)!$1<span class='color_param'>$ParamName</span>$2!ig;
            }
            else
            {
                $Part_Styled =~ s!(\W)$ParamName([\,\)]|\Z)!$1<span style='font-style:italic;'>$ParamName</span>$2!ig;
            }
        }
        $Part_Styled = "<span style='white-space:nowrap;'>".$Part_Styled."</span>";
        push(@Parts, $Part_Styled);
        $Part_Num += 1;
    }
    $Signature = $Begin."<span class='int_p'>"."(".join(" ", @Parts).")"."</span>".$End;
    $Signature =~ s!\[\]![<span style='padding-left:2px;'>]</span>!g;
    $Signature =~ s!operator=!operator<span style='padding-left:2px'>=</span>!g;
    $Signature =~ s!(\[in-charge\]|\[not-in-charge\]|\[in-charge-deleting\])!<span style='color:Black;font-weight:normal;'>$1</span>!g;
    return $Signature.(($SymbolVersion)?"<span class='symver'> \@ $SymbolVersion</span>":"");
}

sub get_Signature_Parts($$)
{
    my ($Signature, $Comma) = @_;
    my @Parts = ();
    my $Bracket_Num = 0;
    my $Bracket2_Num = 0;
    my $Parameters = $Signature;
    if($Signature=~/&gt;|&lt;/)
    {
        $Parameters=~s/&gt;/>/g;
        $Parameters=~s/&lt;/</g;
    }
    my $Part_Num = 0;
    if($Parameters =~ s/.+?\((.*)\)(| const)\Z/$1/)
    {
        foreach my $Pos (0 .. length($Parameters) - 1)
        {
            my $Symbol = substr($Parameters, $Pos, 1);
            $Bracket_Num += 1 if($Symbol eq "(");
            $Bracket_Num -= 1 if($Symbol eq ")");
            $Bracket2_Num += 1 if($Symbol eq "<");
            $Bracket2_Num -= 1 if($Symbol eq ">");
            if($Symbol eq "," and $Bracket_Num==0 and $Bracket2_Num==0)
            {
                $Parts[$Part_Num] .= $Symbol if($Comma);
                $Part_Num += 1;
            }
            else
            {
                $Parts[$Part_Num] .= $Symbol;
            }
        }
        if($Signature=~/&gt;|&lt;/)
        {
            foreach my $Part (@Parts)
            {
                $Part=~s/\>/&gt;/g;
                $Part=~s/\</&lt;/g;
            }
        }
        return @Parts;
    }
    else
    {
        return ();
    }
}

my %TypeProblems_Kind=(
    "Added_Virtual_Function"=>1,
    "Withdrawn_Virtual_Function"=>1,
    "Virtual_Function_Position"=>1,
    "Virtual_Function_Redefinition"=>1,
    "Virtual_Function_Redefinition_B"=>1,
    "Size"=>1,
    "Added_Member_And_Size"=>1,
    "Added_Middle_Member_And_Size"=>1,
    "Withdrawn_Member_And_Size"=>1,
    "Withdrawn_Member"=>1,
    "Withdrawn_Middle_Member_And_Size"=>1,
    "Member_Rename"=>1,
    "Enum_Member_Value"=>1,
    "Enum_Member_Name"=>1,
    "Member_Type_And_Size"=>1,
    "Member_Type"=>1,
    "Member_BaseType_And_Size"=>1,
    "Member_BaseType"=>1,
    "Member_PointerLevel"=>1,
    "BaseType"=>1
);

my %InterfaceProblems_Kind=(
    "Added_Interface"=>1,
    "Withdrawn_Interface"=>1,
    "Function_Become_Static"=>1,
    "Function_Become_NonStatic"=>1,
    "Parameter_Type_And_Size"=>1,
    "Parameter_Type"=>1,
    "Parameter_BaseType_And_Size"=>1,
    "Parameter_BaseType"=>1,
    "Parameter_PointerLevel"=>1,
    "Return_Type_And_Size"=>1,
    "Return_Type"=>1,
    "Return_BaseType_And_Size"=>1,
    "Return_BaseType"=>1,
    "Return_PointerLevel"=>1,
    "Withdrawn_Parameter"=>1,
    "Added_Parameter"=>1
);

sub testSystem_cpp()
{
    print "testing for C++ library changes\n";
    my (@DataDefs_v1, @Sources_v1, @DataDefs_v2, @Sources_v2) = ();
    
    #Withdrawn_Parameter
    @DataDefs_v1 = (@DataDefs_v1, "int func_withdrawn_parameter(int param, int withdrawn_param);");
    @Sources_v1 = (@Sources_v1, "int func_withdrawn_parameter(int param, int withdrawn_param)\n{\n    return 0;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "int func_withdrawn_parameter(int param);");
    @Sources_v2 = (@Sources_v2, "int func_withdrawn_parameter(int param)\n{\n    return 0;\n}");
    
    #Added_Parameter
    @DataDefs_v1 = (@DataDefs_v1, "int func_added_parameter(int param);");
    @Sources_v1 = (@Sources_v1, "int func_added_parameter(int param)\n{\n    return 0;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "int func_added_parameter(int param, int added_param);");
    @Sources_v2 = (@Sources_v2, "int func_added_parameter(int param, int added_param)\n{\n    return 0;\n}");
    
    #Added_Virtual_Function
    @DataDefs_v1 = (@DataDefs_v1, "class type_test_added_virtual_function\n{\npublic:\n    int func1(int param);\n    virtual int func2(int param);\n};");
    @Sources_v1 = (@Sources_v1, "int type_test_added_virtual_function::func1(int param)\n{\n    return param;\n}");
    @Sources_v1 = (@Sources_v1, "int type_test_added_virtual_function::func2(int param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "class type_test_added_virtual_function\n{\npublic:\n    virtual int func1(int param);\n    virtual int func2(int param);\n};");
    @Sources_v2 = (@Sources_v2, "int type_test_added_virtual_function::func1(int param)\n{\n    return param;\n}");
    @Sources_v2 = (@Sources_v2, "int type_test_added_virtual_function::func2(int param)\n{\n    return param;\n}");
    
    #added simple function
    @DataDefs_v2 = (@DataDefs_v2, "typedef int (*FUNCPTR_TYPE)(int a, int b);\nint added_function_param_funcptr(FUNCPTR_TYPE*const** f);");
    @Sources_v2 = (@Sources_v2, "int added_function_param_funcptr(FUNCPTR_TYPE*const** f)\n{\n    return 0;\n}");
    
    #Withdrawn_Virtual_Function
    @DataDefs_v1 = (@DataDefs_v1, "class type_test_withdrawn_virtual_function\n{\npublic:\n    virtual int func1(int param);\n    virtual int func2(int param);\n};");
    @Sources_v1 = (@Sources_v1, "int type_test_withdrawn_virtual_function::func1(int param)\n{\n    return param;\n}");
    @Sources_v1 = (@Sources_v1, "int type_test_withdrawn_virtual_function::func2(int param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "class type_test_withdrawn_virtual_function\n{\npublic:\n    int func1(int param);\n    virtual int func2(int param);\n};");
    @Sources_v2 = (@Sources_v2, "int type_test_withdrawn_virtual_function::func1(int param)\n{\n    return param;\n}");
    @Sources_v2 = (@Sources_v2, "int type_test_withdrawn_virtual_function::func2(int param)\n{\n    return param;\n}");
    
    #Virtual_Function_Position
    @DataDefs_v1 = (@DataDefs_v1, "class type_test_virtual_function_position\n{\npublic:\n    virtual int func1(int param);\n    virtual int func2(int param);\n};");
    @Sources_v1 = (@Sources_v1, "int type_test_virtual_function_position::func1(int param)\n{\n    return param;\n}");
    @Sources_v1 = (@Sources_v1, "int type_test_virtual_function_position::func2(int param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "class type_test_virtual_function_position\n{\npublic:\n    virtual int func2(int param);\n    virtual int func1(int param);\n};");
    @Sources_v2 = (@Sources_v2, "int type_test_virtual_function_position::func1(int param)\n{\n    return param;\n}");
    @Sources_v2 = (@Sources_v2, "int type_test_virtual_function_position::func2(int param)\n{\n    return param;\n}");
    
    #virtual functions safe replace
    @DataDefs_v1 = (@DataDefs_v1, "class type_test_virtual_function_position_safe_replace_base\n{\npublic:\n    virtual int func1(int param);\n    virtual int func2(int param);\n};");
    @Sources_v1 = (@Sources_v1, "int type_test_virtual_function_position_safe_replace_base::func1(int param)\n{\n    return param;\n}");
    @Sources_v1 = (@Sources_v1, "int type_test_virtual_function_position_safe_replace_base::func2(int param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "class type_test_virtual_function_position_safe_replace_base\n{\npublic:\n    virtual int func1(int param);\n    virtual int func2(int param);\n};");
    @Sources_v2 = (@Sources_v2, "int type_test_virtual_function_position_safe_replace_base::func1(int param)\n{\n    return param;\n}");
    @Sources_v2 = (@Sources_v2, "int type_test_virtual_function_position_safe_replace_base::func2(int param)\n{\n    return param;\n}");
    
    @DataDefs_v1 = (@DataDefs_v1, "class type_test_virtual_function_position_safe_replace:public type_test_virtual_function_position_safe_replace_base\n{\npublic:\n    virtual int func1(int param);\n    virtual int func2(int param);\n};");
    @Sources_v1 = (@Sources_v1, "int type_test_virtual_function_position_safe_replace::func1(int param)\n{\n    return param;\n}");
    @Sources_v1 = (@Sources_v1, "int type_test_virtual_function_position_safe_replace::func2(int param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "class type_test_virtual_function_position_safe_replace:public type_test_virtual_function_position_safe_replace_base\n{\npublic:\n    virtual int func2(int param);\n    virtual int func1(int param);\n};");
    @Sources_v2 = (@Sources_v2, "int type_test_virtual_function_position_safe_replace::func1(int param)\n{\n    return param;\n}");
    @Sources_v2 = (@Sources_v2, "int type_test_virtual_function_position_safe_replace::func2(int param)\n{\n    return param;\n}");
    
    #virtual table changes
    @DataDefs_v1 = (@DataDefs_v1, "class type_test_virtual_table_base\n{\npublic:\n    virtual int func1(int param);\n    virtual int func2(int param);\n};");
    @DataDefs_v1 = (@DataDefs_v1, "class type_test_virtual_table:public type_test_virtual_table_base\n{\npublic:\n    virtual int func1(int param);\n    virtual int func2(int param);\n};");
    @Sources_v1 = (@Sources_v1, "int type_test_virtual_table_base::func1(int param)\n{\n    return param;\n}");
    @Sources_v1 = (@Sources_v1, "int type_test_virtual_table_base::func2(int param)\n{\n    return param;\n}");
    @Sources_v1 = (@Sources_v1, "int type_test_virtual_table::func1(int param)\n{\n    return param;\n}");
    @Sources_v1 = (@Sources_v1, "int type_test_virtual_table::func2(int param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "class type_test_virtual_table_base\n{\npublic:\n    virtual int func1(int param);\n    virtual int func2(int param);\n};");
    @DataDefs_v2 = (@DataDefs_v2, "class type_test_virtual_table:public type_test_virtual_table_base\n{\npublic:\n    virtual int func2(int param);\n    virtual int func1(int param);\n};");
    @Sources_v2 = (@Sources_v2, "int type_test_virtual_table_base::func1(int param)\n{\n    return param;\n}");
    @Sources_v2 = (@Sources_v2, "int type_test_virtual_table_base::func2(int param)\n{\n    return param;\n}");
    @Sources_v2 = (@Sources_v2, "int type_test_virtual_table::func1(int param)\n{\n    return param;\n}");
    @Sources_v2 = (@Sources_v2, "int type_test_virtual_table::func2(int param)\n{\n    return param;\n}");
    
    #Virtual_Function_Redefinition
    @DataDefs_v1 = (@DataDefs_v1, "class type_test_virtual_function_redefinition_base\n{\npublic:\n    virtual int func1(int param);\n    virtual int func2(int param);\n};");
    @DataDefs_v1 = (@DataDefs_v1, "class type_test_virtual_function_redefinition:public type_test_virtual_function_redefinition_base\n{\npublic:\n    virtual int func3(int param);\n};");
    @Sources_v1 = (@Sources_v1, "int type_test_virtual_function_redefinition_base::func1(int param)\n{\n    return param;\n}");
    @Sources_v1 = (@Sources_v1, "int type_test_virtual_function_redefinition_base::func2(int param)\n{\n    return param;\n}");
    @Sources_v1 = (@Sources_v1, "int type_test_virtual_function_redefinition::func3(int param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "class type_test_virtual_function_redefinition_base\n{\npublic:\n    virtual int func1(int param);\n    virtual int func2(int param);\n};");
    @DataDefs_v2 = (@DataDefs_v2, "class type_test_virtual_function_redefinition:public type_test_virtual_function_redefinition_base\n{\npublic:\n    virtual int func2(int param);\n    virtual int func3(int param);\n};");
    @Sources_v2 = (@Sources_v2, "int type_test_virtual_function_redefinition_base::func1(int param){\n    return param;\n}");
    @Sources_v2 = (@Sources_v2, "int type_test_virtual_function_redefinition_base::func2(int param){\n    return param;\n}");
    @Sources_v2 = (@Sources_v2, "int type_test_virtual_function_redefinition::func2(int param)\n{\n    return param;\n}");
    @Sources_v2 = (@Sources_v2, "int type_test_virtual_function_redefinition::func3(int param)\n{\n    return param;\n}");
    
    #size change
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_size\n{\npublic:\n    virtual type_test_size func1(type_test_size param);\n    int i;\n    long j;\n    double k;\n    type_test_size* p;\n};");
    @Sources_v1 = (@Sources_v1, "type_test_size type_test_size::func1(type_test_size param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_size\n{\npublic:\n    virtual type_test_size func1(type_test_size param);\n    int i;\n    long j;\n    double k;\n    type_test_size* p;\n    int added_member;\n};");
    @Sources_v2 = (@Sources_v2, "type_test_size type_test_size::func1(type_test_size param)\n{\n    return param;\n}");
    
    #Added_Member_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_added_member\n{\npublic:\n    virtual type_test_added_member func1(type_test_added_member param);\n    int i;\n    long j;\n    double k;\n    type_test_added_member* p;\n};");
    @Sources_v1 = (@Sources_v1, "type_test_added_member type_test_added_member::func1(type_test_added_member param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_added_member\n{\npublic:\n    virtual type_test_added_member func1(type_test_added_member param);\n    int i;\n    long j;\n    double k;\n    type_test_added_member* p;\n    int added_member;\n};");
    @Sources_v2 = (@Sources_v2, "type_test_added_member type_test_added_member::func1(type_test_added_member param)\n{\n    return param;\n}");
    
    #Method object changes
    @DataDefs_v1 = (@DataDefs_v1, "class type_test_object_added_member\n{\npublic:\n    virtual int func1(int param);\n    int i;\n    long j;\n    double k;\n    type_test_added_member* p;\n};");
    @Sources_v1 = (@Sources_v1, "int type_test_object_added_member::func1(int param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "class type_test_object_added_member\n{\npublic:\n    virtual int func1(int param);\n    int i;\n    long j;\n    double k;\n    type_test_added_member* p;\n    int added_member;\n};");
    @Sources_v2 = (@Sources_v2, "int type_test_object_added_member::func1(int param)\n{\n    return param;\n}");
    
    #added bitfield
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_added_bitfield\n{\npublic:\n    virtual type_test_added_bitfield func1(type_test_added_bitfield param);\n    int i;\n    long j;\n    double k;\n    int b1 : 32;\n    int b2 : 31;\n    type_test_added_bitfield* p;\n};");
    @Sources_v1 = (@Sources_v1, "type_test_added_bitfield type_test_added_bitfield::func1(type_test_added_bitfield param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_added_bitfield\n{\npublic:\n    virtual type_test_added_bitfield func1(type_test_added_bitfield param);\n    int i;\n    long j;\n    double k;\n    int b1 : 32;\n    int b2 : 31;\n    int added_bitfield : 1;\n    type_test_added_bitfield* p;\n};");
    @Sources_v2 = (@Sources_v2, "type_test_added_bitfield type_test_added_bitfield::func1(type_test_added_bitfield param)\n{\n    return param;\n}");
    
    #withdrawn bitfield
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_withdrawn_bitfield\n{\npublic:\n    virtual type_test_withdrawn_bitfield func1(type_test_withdrawn_bitfield param);\n    int i;\n    long j;\n    double k;\n    int b1 : 32;\n    int b2 : 31;\n    int withdrawn_bitfield : 1;\n    type_test_withdrawn_bitfield* p;\n};");
    @Sources_v1 = (@Sources_v1, "type_test_withdrawn_bitfield type_test_withdrawn_bitfield::func1(type_test_withdrawn_bitfield param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_withdrawn_bitfield\n{\npublic:\n    virtual type_test_withdrawn_bitfield func1(type_test_withdrawn_bitfield param);\n    int i;\n    long j;\n    double k;\n    int b1 : 32;\n    int b2 : 31;\n    type_test_withdrawn_bitfield* p;\n};");
    @Sources_v2 = (@Sources_v2, "type_test_withdrawn_bitfield type_test_withdrawn_bitfield::func1(type_test_withdrawn_bitfield param)\n{\n    return param;\n}");
    
    #Added_Middle_Member_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_added_middle_member\n{\npublic:\n    virtual type_test_added_middle_member func1(type_test_added_middle_member param);\n    int i;\n    long j;\n    double k;\n    type_test_added_middle_member* p;\n};");
    @Sources_v1 = (@Sources_v1, "type_test_added_middle_member type_test_added_middle_member::func1(type_test_added_middle_member param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_added_middle_member\n{\npublic:\n    virtual type_test_added_middle_member func1(type_test_added_middle_member param);\n    int i;\n    int added_middle_member;\n    long j;\n    double k;\n    type_test_added_middle_member* p;\n};");
    @Sources_v2 = (@Sources_v2, "type_test_added_middle_member type_test_added_middle_member::func1(type_test_added_middle_member param)\n{\n    return param;\n}");
    
    #Member_Rename
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_member_rename\n{\npublic:\n    virtual type_test_member_rename func1(type_test_member_rename param);\n    long i;\n    long j;\n    double k;\n    type_test_member_rename* p;\n};");
    @Sources_v1 = (@Sources_v1, "type_test_member_rename type_test_member_rename::func1(type_test_member_rename param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_member_rename\n{\npublic:\n    virtual type_test_member_rename func1(type_test_member_rename param);\n    long renamed_member;\n    long j;\n    double k;\n    type_test_member_rename* p;\n};");
    @Sources_v2 = (@Sources_v2, "type_test_member_rename type_test_member_rename::func1(type_test_member_rename param)\n{\n    return param;\n}");
    
    #Withdrawn_Member_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_withdrawn_member\n{\npublic:\n    virtual type_test_withdrawn_member func1(type_test_withdrawn_member param);\n    int i;\n    long j;\n    double k;\n    type_test_withdrawn_member* p;\n    int withdrawn_member;\n};");
    @Sources_v1 = (@Sources_v1, "type_test_withdrawn_member type_test_withdrawn_member::func1(type_test_withdrawn_member param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_withdrawn_member\n{\npublic:\n    virtual type_test_withdrawn_member func1(type_test_withdrawn_member param);\n    int i;\n    long j;\n    double k;\n    type_test_withdrawn_member* p;\n};");
    @Sources_v2 = (@Sources_v2, "type_test_withdrawn_member type_test_withdrawn_member::func1(type_test_withdrawn_member param)\n{\n    return param;\n}");
    
    #Withdrawn_Middle_Member_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_withdrawn_middle_member\n{\npublic:\n    virtual type_test_withdrawn_middle_member func1(type_test_withdrawn_middle_member param);\n    int i;\n    int withdrawn_middle_member;\n    long j;\n    double k;\n    type_test_withdrawn_middle_member* p;\n};");
    @Sources_v1 = (@Sources_v1, "type_test_withdrawn_middle_member type_test_withdrawn_middle_member::func1(type_test_withdrawn_middle_member param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_withdrawn_middle_member\n{\npublic:\n    virtual type_test_withdrawn_middle_member func1(type_test_withdrawn_middle_member param);\n    int i;\n    long j;\n    double k;\n    type_test_withdrawn_middle_member* p;\n};");
    @Sources_v2 = (@Sources_v2, "type_test_withdrawn_middle_member type_test_withdrawn_middle_member::func1(type_test_withdrawn_middle_member param)\n{\n    return param;\n}");
    
    #Enum_Member_Value
    @DataDefs_v1 = (@DataDefs_v1, "enum type_test_enum_member_value_change\n{\n    MEMBER_1=1,\n    MEMBER_2=2\n};");
    @DataDefs_v1 = (@DataDefs_v1, "int func_test_enum_member_value_change(enum type_test_enum_member_value_change param);");
    @Sources_v1 = (@Sources_v1,
"int func_test_enum_member_value_change(enum type_test_enum_member_value_change param)
{
    switch(param)
    {
        case 1:
            return 1;
        case 2:
            return 2;
    }
    return 0;
}");
    
    @DataDefs_v2 = (@DataDefs_v2, "enum type_test_enum_member_value_change\n{\n    MEMBER_1=2,\n    MEMBER_2=1\n};");
    @DataDefs_v2 = (@DataDefs_v2, "int func_test_enum_member_value_change(enum type_test_enum_member_value_change param);");
    @Sources_v2 = (@Sources_v2,
"int func_test_enum_member_value_change(enum type_test_enum_member_value_change param)
{
    switch(param)
    {
        case 1:
            return 1;
        case 2:
            return 2;
    }
    return 0;
}");
    
    #Enum_Member_Name
    @DataDefs_v1 = (@DataDefs_v1, "enum type_test_enum_member_rename\n{\n    BRANCH_1=1,\n    BRANCH_2=2\n};");
    @DataDefs_v1 = (@DataDefs_v1, "int func_test_enum_member_rename(enum type_test_enum_member_rename param);");
    @Sources_v1 = (@Sources_v1,
"int func_test_enum_member_rename(enum type_test_enum_member_rename param)
{
    switch(param)
    {
        case 1:
            return 1;
        case 2:
            return 2;
    }
    return 0;
}");
    
    @DataDefs_v2 = (@DataDefs_v2, "enum type_test_enum_member_rename\n{\n    BRANCH_FIRST=1,\n    BRANCH_SECOND=1\n};");
    @DataDefs_v2 = (@DataDefs_v2, "int func_test_enum_member_rename(enum type_test_enum_member_rename param);");
    @Sources_v2 = (@Sources_v2,
"int func_test_enum_member_rename(enum type_test_enum_member_rename param)
{
    switch(param)
    {
        case 1:
            return 1;
        case 2:
            return 2;
    }
    return 0;
}");
    
    #Member_Type_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_member_type_and_size\n{\npublic:\n    type_test_member_type_and_size func1(type_test_member_type_and_size param);\n    int i;\n    long j;\n    double k;\n    type_test_member_type_and_size* p;\n};");
    @Sources_v1 = (@Sources_v1, "type_test_member_type_and_size type_test_member_type_and_size::func1(type_test_member_type_and_size param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_member_type_and_size\n{\npublic:\n    type_test_member_type_and_size func1(type_test_member_type_and_size param);\n    long long i;\n    long j;\n    double k;\n    type_test_member_type_and_size* p;\n};");
    @Sources_v2 = (@Sources_v2, "type_test_member_type_and_size type_test_member_type_and_size::func1(type_test_member_type_and_size param)\n{\n    return param;\n}");
    
    #Member_Type
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_member_type\n{\npublic:\n    type_test_member_type func1(type_test_member_type param);\n    int i;\n    long j;\n    double k;\n    type_test_member_type* p;\n};");
    @Sources_v1 = (@Sources_v1, "type_test_member_type type_test_member_type::func1(type_test_member_type param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_member_type\n{\npublic:\n    type_test_member_type func1(type_test_member_type param);\n    float i;\n    long j;\n    double k;\n    type_test_member_type* p;\n};");
    @Sources_v2 = (@Sources_v2, "type_test_member_type type_test_member_type::func1(type_test_member_type param)\n{\n    return param;\n}");
    
    #Member_BaseType
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_member_basetype\n{\npublic:\n    type_test_member_basetype func1(type_test_member_basetype param);\n    int *i;\n    long j;\n    double k;\n    type_test_member_basetype* p;\n};");
    @Sources_v1 = (@Sources_v1, "type_test_member_basetype type_test_member_basetype::func1(type_test_member_basetype param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_member_basetype\n{\npublic:\n    type_test_member_basetype func1(type_test_member_basetype param);\n    long long *i;\n    long j;\n    double k;\n    type_test_member_basetype* p;\n};");
    @Sources_v2 = (@Sources_v2, "type_test_member_basetype type_test_member_basetype::func1(type_test_member_basetype param)\n{\n    return param;\n}");
    
    #Member_PointerLevel_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_member_pointerlevel_and_size\n{\npublic:\n    type_test_member_pointerlevel_and_size func1(type_test_member_pointerlevel_and_size param);\n    long long i;\n    long j;\n    double k;\n    type_test_member_pointerlevel_and_size* p;\n};");
    @Sources_v1 = (@Sources_v1, "type_test_member_pointerlevel_and_size type_test_member_pointerlevel_and_size::func1(type_test_member_pointerlevel_and_size param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_member_pointerlevel_and_size\n{\npublic:\n    type_test_member_pointerlevel_and_size func1(type_test_member_pointerlevel_and_size param);\n    long long *i;\n    long j;\n    double k;\n    type_test_member_pointerlevel_and_size* p;\n};");
    @Sources_v2 = (@Sources_v2, "type_test_member_pointerlevel_and_size type_test_member_pointerlevel_and_size::func1(type_test_member_pointerlevel_and_size param)\n{\n    return param;\n}");
    
    #Member_PointerLevel
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_member_pointerlevel\n{\npublic:\n    type_test_member_pointerlevel func1(type_test_member_pointerlevel param);\n    int **i;\n    long j;\n    double k;\n    type_test_member_pointerlevel* p;\n};");
    @Sources_v1 = (@Sources_v1, "type_test_member_pointerlevel type_test_member_pointerlevel::func1(type_test_member_pointerlevel param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_member_pointerlevel\n{\npublic:\n    type_test_member_pointerlevel func1(type_test_member_pointerlevel param);\n    int *i;\n    long j;\n    double k;\n    type_test_member_pointerlevel* p;\n};");
    @Sources_v2 = (@Sources_v2, "type_test_member_pointerlevel type_test_member_pointerlevel::func1(type_test_member_pointerlevel param)\n{\n    return param;\n}");
    
    #Added_Interface (function)
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_added_interface\n{\npublic:\n    type_test_added_interface func1(type_test_added_interface param);\n    int i;\n    long j;\n    double k;\n    type_test_added_interface* p;\n};");
    @Sources_v1 = (@Sources_v1, "type_test_added_interface type_test_added_interface::func1(type_test_added_interface param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_added_interface\n{\npublic:\n    type_test_added_interface func1(type_test_added_interface param);\n    type_test_added_interface added_func(type_test_added_interface param);\n    int i;\n    long j;\n    double k;\n    type_test_added_interface* p;\n};");
    @DataDefs_v2 = (@DataDefs_v2, "int added_func_2(void *** param);");
    @Sources_v2 = (@Sources_v2, "type_test_added_interface type_test_added_interface::func1(type_test_added_interface param)\n{\n    return param;\n}");
    @Sources_v2 = (@Sources_v2, "type_test_added_interface type_test_added_interface::added_func(type_test_added_interface param)\n{\n    return param;\n}");
    @Sources_v2 = (@Sources_v2, "int added_func_2(void *** param)\n{\n    return 0;\n}");
    
    #Added_Interface (global variable)
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_added_variable\n{\npublic:\n    int func1(type_test_added_variable param);\n    int i;\n    long j;\n    double k;\n    type_test_added_variable* p;\n};");
    @Sources_v1 = (@Sources_v1, "int type_test_added_variable::func1(type_test_added_variable param)\n{\n    return i;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_added_variable\n{\npublic:\n    int func1(type_test_added_variable param);\n    static int i;\n    long j;\n    double k;\n    type_test_added_variable* p;\n};");
    @Sources_v2 = (@Sources_v2, "int type_test_added_variable::func1(type_test_added_variable param)\n{\n    return type_test_added_variable::i;\n}");
    @Sources_v2 = (@Sources_v2, "int type_test_added_variable::i=0;");
    
    #Withdrawn_Interface
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_withdrawn_interface\n{\npublic:\n    type_test_withdrawn_interface func1(type_test_withdrawn_interface param);\n    type_test_withdrawn_interface withdrawn_func(type_test_withdrawn_interface param);\n    int i;\n    long j;\n    double k;\n    type_test_withdrawn_interface* p;\n};");
    @DataDefs_v1 = (@DataDefs_v1, "int withdrawn_func_2(void *** param);");
    @Sources_v1 = (@Sources_v1, "type_test_withdrawn_interface type_test_withdrawn_interface::func1(type_test_withdrawn_interface param)\n{\n    return param;\n}");
    @Sources_v1 = (@Sources_v1, "type_test_withdrawn_interface type_test_withdrawn_interface::withdrawn_func(type_test_withdrawn_interface param)\n{\n    return param;\n}");
    @Sources_v1 = (@Sources_v1, "int withdrawn_func_2(void *** param)\n{\n    return 0;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_withdrawn_interface\n{\npublic:\n    type_test_withdrawn_interface func1(type_test_withdrawn_interface param);\n    int i;\n    long j;\n    double k;\n    type_test_withdrawn_interface* p;\n};");
    @Sources_v2 = (@Sources_v2, "type_test_withdrawn_interface type_test_withdrawn_interface::func1(type_test_withdrawn_interface param)\n{\n    return param;\n}");
    
    #Function_Become_Static
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_become_static\n{\npublic:\n    type_test_become_static func_become_static(type_test_become_static param);\n    int **i;\n    long j;\n    double k;\n    type_test_become_static* p;\n};");
    @Sources_v1 = (@Sources_v1, "type_test_become_static type_test_become_static::func_become_static(type_test_become_static param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_become_static\n{\npublic:\n    static type_test_become_static func_become_static(type_test_become_static param);\n    int **i;\n    long j;\n    double k;\n    type_test_become_static* p;\n};");
    @Sources_v2 = (@Sources_v2, "type_test_become_static type_test_become_static::func_become_static(type_test_become_static param)\n{\n    return param;\n}");
    
    #Function_Become_NonStatic
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_become_nonstatic\n{\npublic:\n    static type_test_become_nonstatic func_become_nonstatic(type_test_become_nonstatic param);\n    int **i;\n    long j;\n    double k;\n    type_test_become_nonstatic* p;\n};");
    @Sources_v1 = (@Sources_v1, "type_test_become_nonstatic type_test_become_nonstatic::func_become_nonstatic(type_test_become_nonstatic param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_become_nonstatic\n{\npublic:\n    type_test_become_nonstatic func_become_nonstatic(type_test_become_nonstatic param);\n    int **i;\n    long j;\n    double k;\n    type_test_become_nonstatic* p;\n};");
    @Sources_v2 = (@Sources_v2, "type_test_become_nonstatic type_test_become_nonstatic::func_become_nonstatic(type_test_become_nonstatic param)\n{\n    return param;\n}");
    
    #Parameter_Type_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "int func_parameter_type_and_size(int param, int other_param);");
    @Sources_v1 = (@Sources_v1, "int func_parameter_type_and_size(int param, int other_param)\n{\n    return other_param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "int func_parameter_type_and_size(long long param, int other_param);");
    @Sources_v2 = (@Sources_v2, "int func_parameter_type_and_size(long long param, int other_param)\n{\n    return other_param;\n}");
    
    #Parameter_Type
    @DataDefs_v1 = (@DataDefs_v1, "int func_parameter_type(int param, int other_param);");
    @Sources_v1 = (@Sources_v1, "int func_parameter_type(int param, int other_param)\n{\n    return other_param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "int func_parameter_type(float param, int other_param);");
    @Sources_v2 = (@Sources_v2, "int func_parameter_type(float param, int other_param)\n{\n    return other_param;\n}");
    
    #Parameter_BaseType
    @DataDefs_v1 = (@DataDefs_v1, "int func_parameter_basetypechange(int *param);");
    @Sources_v1 = (@Sources_v1, "int func_parameter_basetypechange(int *param)\n{\n    return sizeof(*param);\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "int func_parameter_basetypechange(long long *param);");
    @Sources_v2 = (@Sources_v2, "int func_parameter_basetypechange(long long *param)\n{\n    return sizeof(*param);\n}");
    
    #Parameter_PointerLevel_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "long long func_parameter_pointerlevel_and_size(long long param);");
    @Sources_v1 = (@Sources_v1, "long long func_parameter_pointerlevel_and_size(long long param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "long long func_parameter_pointerlevel_and_size(long long *param);");
    @Sources_v2 = (@Sources_v2, "long long func_parameter_pointerlevel_and_size(long long *param)\n{\n    return param[5];\n}");
    
    #Parameter_PointerLevel
    @DataDefs_v1 = (@DataDefs_v1, "int func_parameter_pointerlevel(int *param);");
    @Sources_v1 = (@Sources_v1, "int func_parameter_pointerlevel(int *param)\n{\n    return param[5];\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "int func_parameter_pointerlevel(int **param);");
    @Sources_v2 = (@Sources_v2, "int func_parameter_pointerlevel(int **param)\n{\n    return param[5][5];\n}");
    
    #Return_Type_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "int func_return_type_and_size(int param);");
    @Sources_v1 = (@Sources_v1, "int func_return_type_and_size(int param)\n{\n    return 2^(sizeof(int)*8-1)-1;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "long long func_return_type_and_size(int param);");
    @Sources_v2 = (@Sources_v2, "long long func_return_type_and_size(int param)\n{\n    return 2^(sizeof(long long)*8-1)-1;\n}");
    
    #Return_Type
    @DataDefs_v1 = (@DataDefs_v1, "int func_return_type(int param);");
    @Sources_v1 = (@Sources_v1, "int func_return_type(int param)\n{\n    return 2^(sizeof(int)*8-1)-1;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "float func_return_type(int param);");
    @Sources_v2 = (@Sources_v2, "float func_return_type(int param)\n{\n    return 0.7;\n}");
    
    #Return_BaseType
    @DataDefs_v1 = (@DataDefs_v1, "int *func_return_basetype(int param);");
    @Sources_v1 = (@Sources_v1, "int *func_return_basetype(int param)\n{\n    int *x = new int[10];\n    return x;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "long long *func_return_basetype(int param);");
    @Sources_v2 = (@Sources_v2, "long long *func_return_basetype(int param)\n{\n    long long *x = new long long[10];\n    return x;\n}");
    
    #Return_PointerLevel_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "long long func_return_pointerlevel_and_size(int param);");
    @Sources_v1 = (@Sources_v1, "long long func_return_pointerlevel_and_size(int param)\n{\n    return 100;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "long long *func_return_pointerlevel_and_size(int param);");
    @Sources_v2 = (@Sources_v2, "long long *func_return_pointerlevel_and_size(int param)\n{\n    long long* x = new long long[10];\n    return x;\n}");
    
    #Return_PointerLevel
    @DataDefs_v1 = (@DataDefs_v1, "int* func_return_pointerlevel(int param);");
    @Sources_v1 = (@Sources_v1, "int* func_return_pointerlevel(int param)\n{\n    int* x = new int[10];\n    return x;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "int **func_return_pointerlevel(int param);");
    @Sources_v2 = (@Sources_v2, "int **func_return_pointerlevel(int param)\n{\n    int** x = new int*[10];\n    return x;\n}");
    
    #typedef to anon struct
    @DataDefs_v1 = (@DataDefs_v1, "
typedef struct
{
public:
    int i;
    long j;
    double k;
} type_test_anon_typedef;
int func_test_anon_typedef(type_test_anon_typedef param);");
    @Sources_v1 = (@Sources_v1, "int func_test_anon_typedef(type_test_anon_typedef param)\n{\n    return 0;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "
typedef struct
{
public:
    int i;
    long j;
    double k;
    union {
        int dummy[256];
        struct {
            char q_skiptable[256];
            const char *p;
            int l;
        } p;
    };
} type_test_anon_typedef;
int func_test_anon_typedef(type_test_anon_typedef param);");
    @Sources_v2 = (@Sources_v2, "int func_test_anon_typedef(type_test_anon_typedef param)\n{\n    return 0;\n}");
    
    #opaque type
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_opaque\n{\npublic:\n    virtual type_test_opaque func1(type_test_opaque param);\n    int i;\n    long j;\n    double k;\n    type_test_opaque* p;\n};");
    @Sources_v1 = (@Sources_v1, "type_test_opaque type_test_opaque::func1(type_test_opaque param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_opaque\n{\npublic:\n    virtual type_test_opaque func1(type_test_opaque param);\n    int i;\n    long j;\n    double k;\n    type_test_opaque* p;\n    int added_member;\n};");
    @Sources_v2 = (@Sources_v2, "type_test_opaque type_test_opaque::func1(type_test_opaque param)\n{\n    return param;\n}");
    
    #internal function
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_internal\n{\npublic:\n    virtual type_test_internal func1(type_test_internal param);\n    int i;\n    long j;\n    double k;\n    type_test_internal* p;\n};");
    @Sources_v1 = (@Sources_v1, "type_test_internal type_test_internal::func1(type_test_internal param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_internal\n{\npublic:\n    virtual type_test_internal func1(type_test_internal param);\n    int i;\n    long j;\n    double k;\n    type_test_internal* p;\n    int added_member;\n};");
    @Sources_v2 = (@Sources_v2, "type_test_internal type_test_internal::func1(type_test_internal param)\n{\n    return param;\n}");
    
    #starting versioning
    @DataDefs_v1 = (@DataDefs_v1, "int func_return_type_and_start_versioning(int param);");
    @Sources_v1 = (@Sources_v1, "int func_return_type_and_start_versioning(int param)\n{\n    return 2^(sizeof(int)*8-1)-1;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "int func_return_type_and_start_versioning(int param);");
    @Sources_v2 = (@Sources_v2, "int func_return_type_and_start_versioning(int param)\n{\n    return 2^(sizeof(int)*8-1)-1;\n}\n__asm__(\".symver _Z37func_return_type_and_start_versioningi,_Z37func_return_type_and_start_versioningi\@TEST_2.0\");");
    
    #Return_Type And Good Versioning
    @DataDefs_v1 = (@DataDefs_v1, "int func_return_type_and_good_versioning(int param);");
    @Sources_v1 = (@Sources_v1, "int func_return_type_and_good_versioning(int param)\n{\n    return 2^(sizeof(int)*8-1)-1;\n}\n__asm__(\".symver _Z36func_return_type_and_good_versioningi,_Z36func_return_type_and_good_versioningi\@TEST_1.0\");");
    
    @DataDefs_v2 = (@DataDefs_v2, "int func_return_type_and_good_versioning_old(int param);");
    @Sources_v2 = (@Sources_v2, "int func_return_type_and_good_versioning_old(int param)\n{\n    return 2^(sizeof(int)*8-1)-1;\n}\n__asm__(\".symver _Z40func_return_type_and_good_versioning_oldi,_Z36func_return_type_and_good_versioningi\@TEST_1.0\");");
    
    @DataDefs_v2 = (@DataDefs_v2, "float func_return_type_and_good_versioning(int param);");
    @Sources_v2 = (@Sources_v2, "float func_return_type_and_good_versioning(int param)\n{\n    return 0.7;\n}\n__asm__(\".symver _Z36func_return_type_and_good_versioningi,_Z36func_return_type_and_good_versioningi\@TEST_2.0\");");
    
    #Return_Type and bad versioning
    @DataDefs_v1 = (@DataDefs_v1, "int func_return_type_and_bad_versioning(int param);");
    @Sources_v1 = (@Sources_v1, "int func_return_type_and_bad_versioning(int param)\n{\n    return 2^(sizeof(int)*8-1)-1;\n}\n__asm__(\".symver _Z35func_return_type_and_bad_versioningi,_Z35func_return_type_and_bad_versioningi\@TEST_1.0\");");
    
    @DataDefs_v2 = (@DataDefs_v2, "float func_return_type_and_bad_versioning_old(int param);");
    @Sources_v2 = (@Sources_v2, "float func_return_type_and_bad_versioning_old(int param)\n{\n    return 0.7;\n}\n__asm__(\".symver _Z39func_return_type_and_bad_versioning_oldi,_Z35func_return_type_and_bad_versioningi\@TEST_1.0\");");
    
    @DataDefs_v2 = (@DataDefs_v2, "float func_return_type_and_bad_versioning(int param);");
    @Sources_v2 = (@Sources_v2, "float func_return_type_and_bad_versioning(int param)\n{\n    return 0.7;\n}\n__asm__(\".symver _Z35func_return_type_and_bad_versioningi,_Z35func_return_type_and_bad_versioningi\@TEST_2.0\");");
    
    #unnamed struct fields within structs
    @DataDefs_v1 = (@DataDefs_v1, "
typedef struct {
  int a;
  struct {
    int b;
    float c;
  };
  int d;
} type_test_unnamed;
int func_test_unnamed(type_test_unnamed param);");
    @Sources_v1 = (@Sources_v1, "int func_test_unnamed(type_test_unnamed param)\n{\n    return 0;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "
typedef struct {
  int a;
  struct {
    long double b;
    float c;
  };
  int d;
} type_test_unnamed;
int func_test_unnamed(type_test_unnamed param);");
    @Sources_v2 = (@Sources_v2, "int func_test_unnamed(type_test_unnamed param)\n{\n    return 0;\n}");
    
    #constants
    @DataDefs_v1 = (@DataDefs_v1, "#define TEST_PUBLIC_CONSTANT \"old_value\"");
    @DataDefs_v2 = (@DataDefs_v2, "#define TEST_PUBLIC_CONSTANT \"new_value\"");
    
    @DataDefs_v1 = (@DataDefs_v1, "#define TEST_PRIVATE_CONSTANT \"old_value\"\n#undef TEST_PRIVATE_CONSTANT");
    @DataDefs_v2 = (@DataDefs_v2, "#define TEST_PRIVATE_CONSTANT \"new_value\"\n#undef TEST_PRIVATE_CONSTANT");
    
    #unions
    @DataDefs_v1 = (@DataDefs_v1, "
union type_test_union {
  int a;
  struct {
    int b;
    float c;
  };
  int d;
};
int func_test_union(type_test_union param);");
    @Sources_v1 = (@Sources_v1, "int func_test_union(type_test_union param)\n{\n    return 0;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "
union type_test_union {
  int a;
  long double new_member;
  struct {
    int b;
    float c;
  };
  int d;
};
int func_test_union(type_test_union param);");
    @Sources_v2 = (@Sources_v2, "int func_test_union(type_test_union param)\n{\n    return 0;\n}");
    
    #typedefs
    @DataDefs_v1 = (@DataDefs_v1, "typedef float TYPEDEF_TYPE;\nint func_parameter_typedef_change(TYPEDEF_TYPE param);");
    @Sources_v1 = (@Sources_v1, "int func_parameter_typedef_change(TYPEDEF_TYPE param)\n{\n    return 1;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "typedef int TYPEDEF_TYPE;\nint func_parameter_typedef_change(TYPEDEF_TYPE param);");
    @Sources_v2 = (@Sources_v2, "int func_parameter_typedef_change(TYPEDEF_TYPE param)\n{\n    return 1;\n}");
    
    #typedefs in member type
    @DataDefs_v1 = (@DataDefs_v1, "typedef float TYPEDEF_TYPE_2;\nstruct type_test_member_typedef_change{\n    public:\nTYPEDEF_TYPE_2 m;};\nint func_test_member_typedef_change(type_test_member_typedef_change param);");
    @Sources_v1 = (@Sources_v1, "int func_test_member_typedef_change(type_test_member_typedef_change param)\n{\n    return 1;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "typedef int TYPEDEF_TYPE_2;\nstruct type_test_member_typedef_change{\n    public:\nTYPEDEF_TYPE_2 m;};\nint func_test_member_typedef_change(type_test_member_typedef_change param);");
    @Sources_v2 = (@Sources_v2, "int func_test_member_typedef_change(type_test_member_typedef_change param)\n{\n    return 1;\n}");
    
    create_TestSuite("abi_changes_test_cpp", "C++", join("\n\n", @DataDefs_v1), join("\n\n", @Sources_v1), join("\n\n", @DataDefs_v2), join("\n\n", @Sources_v2), "type_test_opaque", "_ZN18type_test_internal5func1ES_");
}

sub testSystem_c()
{
    print "\ntesting for C library changes\n";
    my (@DataDefs_v1, @Sources_v1, @DataDefs_v2, @Sources_v2) = ();
    
    #Withdrawn_Parameter
    @DataDefs_v1 = (@DataDefs_v1, "int func_withdrawn_parameter(int param, int withdrawn_param);");
    @Sources_v1 = (@Sources_v1, "int func_withdrawn_parameter(int param, int withdrawn_param)\n{\n    return 0;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "int func_withdrawn_parameter(int param);");
    @Sources_v2 = (@Sources_v2, "int func_withdrawn_parameter(int param)\n{\n    return 0;\n}");
    
    #Added_Parameter
    @DataDefs_v1 = (@DataDefs_v1, "int func_added_parameter(int param);");
    @Sources_v1 = (@Sources_v1, "int func_added_parameter(int param)\n{\n    return 0;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "int func_added_parameter(int param, int added_param);");
    @Sources_v2 = (@Sources_v2, "int func_added_parameter(int param, int added_param)\n{\n    return 0;\n}");
    
    #added function with typedef funcptr parameter
    @DataDefs_v2 = (@DataDefs_v2, "typedef int (*FUNCPTR_TYPE)(int a, int b);\nint added_function_param_typedef_funcptr(FUNCPTR_TYPE*const** f);");
    @Sources_v2 = (@Sources_v2, "int added_function_param_typedef_funcptr(FUNCPTR_TYPE*const** f)\n{\n    return 0;\n}");
    
    #added function with funcptr parameter
    @DataDefs_v2 = (@DataDefs_v2, "int added_function_param_funcptr(int(*func)(int, int));");
    @Sources_v2 = (@Sources_v2, "int added_function_param_funcptr(int(*func)(int, int))\n{\n    return 0;\n}");
    
    #added function with no limited parameters
    @DataDefs_v2 = (@DataDefs_v2, "int added_function_nolimit_param(float p1, ...);");
    @Sources_v2 = (@Sources_v2, "int added_function_nolimit_param(float p1, ...)\n{\n    return 0;\n}");
    
    #size change
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_size\n{\n    long long i[5];\n    long j;\n    double k;\n    struct type_test_size* p;\n};");
    @DataDefs_v1 = (@DataDefs_v1, "int func_test_type_size(struct type_test_size param, int param_2);");
    @Sources_v1 = (@Sources_v1, "int func_test_type_size(struct type_test_size param, int param_2)\n{\n    return param_2;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_size\n{\n    long long i[5];\n    long long j;\n    double k;\n    struct type_test_size* p;\n};");
    @DataDefs_v2 = (@DataDefs_v2, "int func_test_type_size(struct type_test_size param, int param_2);");
    @Sources_v2 = (@Sources_v2, "int func_test_type_size(struct type_test_size param, int param_2)\n{\n    return param_2;\n}");
    
    #Added_Member_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_added_member\n{\n    int i;\n    long j;\n    double k;\n    struct type_test_added_member* p;\n};");
    @DataDefs_v1 = (@DataDefs_v1, "int func_test_added_member(struct type_test_added_member param, int param_2);");
    @Sources_v1 = (@Sources_v1, "int func_test_added_member(struct type_test_added_member param, int param_2)\n{\n    return param_2;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_added_member\n{\n    int i;\n    long j;\n    double k;\n    struct type_test_added_member* p;\n    int added_member;\n};");
    @DataDefs_v2 = (@DataDefs_v2, "int func_test_added_member(struct type_test_added_member param, int param_2);");
    @Sources_v2 = (@Sources_v2, "int func_test_added_member(struct type_test_added_member param, int param_2)\n{\n    return param_2;\n}");
    
    #Added_Middle_Member_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_added_middle_member\n{\n    int i;\n    long j;\n    double k;\n    struct type_test_added_member* p;\n};");
    @DataDefs_v1 = (@DataDefs_v1, "int func_test_added_middle_member(struct type_test_added_middle_member param, int param_2);");
    @Sources_v1 = (@Sources_v1, "int func_test_added_middle_member(struct type_test_added_middle_member param, int param_2)\n{\n    return param_2;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_added_middle_member\n{\n    int i;\n    int added_middle_member;\n    long j;\n    double k;\n    struct type_test_added_member* p;\n};");
    @DataDefs_v2 = (@DataDefs_v2, "int func_test_added_middle_member(struct type_test_added_middle_member param, int param_2);");
    @Sources_v2 = (@Sources_v2, "int func_test_added_middle_member(struct type_test_added_middle_member param, int param_2)\n{\n    return param_2;\n}");
    
    #Member_Rename
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_member_rename\n{\n    long i;\n    long j;\n    double k;\n    struct type_test_member_rename* p;\n};");
    @DataDefs_v1 = (@DataDefs_v1, "int func_test_member_rename(struct type_test_member_rename param, int param_2);");
    @Sources_v1 = (@Sources_v1, "int func_test_member_rename(struct type_test_member_rename param, int param_2)\n{\n    return param_2;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_member_rename\n{\n    long renamed_member;\n    long j;\n    double k;\n    struct type_test_member_rename* p;\n};");
    @DataDefs_v2 = (@DataDefs_v2, "int func_test_member_rename(struct type_test_member_rename param, int param_2);");
    @Sources_v2 = (@Sources_v2, "int func_test_member_rename(struct type_test_member_rename param, int param_2)\n{\n    return param_2;\n}");
    
    #Withdrawn_Member_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_withdrawn_member\n{\n    int i;\n    long j;\n    double k;\n    struct type_test_withdrawn_member* p;\n    int withdrawn_member;\n};");
    @DataDefs_v1 = (@DataDefs_v1, "int func_test_withdrawn_member(struct type_test_withdrawn_member param, int param_2);");
    @Sources_v1 = (@Sources_v1, "int func_test_withdrawn_member(struct type_test_withdrawn_member param, int param_2)\n{\n    return param_2;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_withdrawn_member\n{\n    int i;\n    long j;\n    double k;\n    struct type_test_withdrawn_member* p;\n};");
    @DataDefs_v2 = (@DataDefs_v2, "int func_test_withdrawn_member(struct type_test_withdrawn_member param, int param_2);");
    @Sources_v2 = (@Sources_v2, "int func_test_withdrawn_member(struct type_test_withdrawn_member param, int param_2)\n{\n    return param_2;\n}");
    
    #Withdrawn_Middle_Member
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_withdrawn_middle_member\n{\n    int i;\n    int withdrawn_middle_member;\n    long j;\n    double k;\n    struct type_test_withdrawn_middle_member* p;\n};");
    @DataDefs_v1 = (@DataDefs_v1, "int func_test_withdrawn_middle_member(struct type_test_withdrawn_middle_member param, int param_2);");
    @Sources_v1 = (@Sources_v1, "int func_test_withdrawn_middle_member(struct type_test_withdrawn_middle_member param, int param_2)\n{\n    return param_2;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_withdrawn_middle_member\n{\n    int i;\n    long j;\n    double k;\n    struct type_test_withdrawn_middle_member* p;\n};");
    @DataDefs_v2 = (@DataDefs_v2, "int func_test_withdrawn_middle_member(struct type_test_withdrawn_middle_member param, int param_2);");
    @Sources_v2 = (@Sources_v2, "int func_test_withdrawn_middle_member(struct type_test_withdrawn_middle_member param, int param_2)\n{\n    return param_2;\n}");
    
    #Enum_Member_Value
    @DataDefs_v1 = (@DataDefs_v1, "enum type_test_enum_member_value_change\n{\n    MEMBER_1=1,\n    MEMBER_2=2\n};");
    @DataDefs_v1 = (@DataDefs_v1, "int func_test_enum_member_value_change(enum type_test_enum_member_value_change param);");
    @Sources_v1 = (@Sources_v1,
"int func_test_enum_member_value_change(enum type_test_enum_member_value_change param)
{
    switch(param)
    {
        case 1:
            return 1;
        case 2:
            return 2;
    }
    return 0;
}");
    
    @DataDefs_v2 = (@DataDefs_v2, "enum type_test_enum_member_value_change\n{\n    MEMBER_1=2,\n    MEMBER_2=1\n};");
    @DataDefs_v2 = (@DataDefs_v2, "int func_test_enum_member_value_change(enum type_test_enum_member_value_change param);");
    @Sources_v2 = (@Sources_v2,
"int func_test_enum_member_value_change(enum type_test_enum_member_value_change param)
{
    switch(param)
    {
        case 1:
            return 1;
        case 2:
            return 2;
    }
    return 0;
}");
    
    #Enum_Member_Name
    @DataDefs_v1 = (@DataDefs_v1, "enum type_test_enum_member_rename\n{\n    BRANCH_1=1,\n    BRANCH_2=2\n};");
    @DataDefs_v1 = (@DataDefs_v1, "int func_test_enum_member_rename(enum type_test_enum_member_rename param);");
    @Sources_v1 = (@Sources_v1,
"int func_test_enum_member_rename(enum type_test_enum_member_rename param)
{
    switch(param)
    {
        case 1:
            return 1;
        case 2:
            return 2;
    }
    return 0;
}");
    
    @DataDefs_v2 = (@DataDefs_v2, "enum type_test_enum_member_rename\n{\n    BRANCH_FIRST=1,\n    BRANCH_SECOND=1\n};");
    @DataDefs_v2 = (@DataDefs_v2, "int func_test_enum_member_rename(enum type_test_enum_member_rename param);");
    @Sources_v2 = (@Sources_v2,
"int func_test_enum_member_rename(enum type_test_enum_member_rename param)
{
    switch(param)
    {
        case 1:
            return 1;
        case 2:
            return 2;
    }
    return 0;
}");
    
    #Member_Type_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_member_type_and_size\n{\n    int i;\n    long j;\n    double k;\n    struct type_test_member_type_and_size* p;\n};");
    @DataDefs_v1 = (@DataDefs_v1, "int func_test_member_type_and_size(struct type_test_member_type_and_size param, int param_2);");
    @Sources_v1 = (@Sources_v1, "int func_test_member_type_and_size(struct type_test_member_type_and_size param, int param_2)\n{\n    return param_2;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_member_type_and_size\n{\n    int i;\n    long j;\n    long double k;\n    struct type_test_member_type_and_size* p;\n};");
    @DataDefs_v2 = (@DataDefs_v2, "int func_test_member_type_and_size(struct type_test_member_type_and_size param, int param_2);");
    @Sources_v2 = (@Sources_v2, "int func_test_member_type_and_size(struct type_test_member_type_and_size param, int param_2)\n{\n    return param_2;\n}");
    
    #Member_Type
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_member_type\n{\n    int i;\n    long j;\n    double k;\n    struct type_test_member_type* p;\n};");
    @DataDefs_v1 = (@DataDefs_v1, "int func_test_member_type(struct type_test_member_type param, int param_2);");
    @Sources_v1 = (@Sources_v1, "int func_test_member_type(struct type_test_member_type param, int param_2)\n{\n    return param_2;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_member_type\n{\n    float i;\n    long j;\n    double k;\n    struct type_test_member_type* p;\n};");
    @DataDefs_v2 = (@DataDefs_v2, "int func_test_member_type(struct type_test_member_type param, int param_2);");
    @Sources_v2 = (@Sources_v2, "int func_test_member_type(struct type_test_member_type param, int param_2)\n{\n    return param_2;\n}");
    
    #Member_BaseType
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_member_basetype\n{\n    int i;\n    long *j;\n    double k;\n    struct type_test_member_basetype* p;\n};");
    @DataDefs_v1 = (@DataDefs_v1, "int func_test_member_basetype(struct type_test_member_basetype param, int param_2);");
    @Sources_v1 = (@Sources_v1, "int func_test_member_basetype(struct type_test_member_basetype param, int param_2)\n{\n    return param_2;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_member_basetype\n{\n    int i;\n    long long *j;\n    double k;\n    struct type_test_member_basetype* p;\n};");
    @DataDefs_v2 = (@DataDefs_v2, "int func_test_member_basetype(struct type_test_member_basetype param, int param_2);");
    @Sources_v2 = (@Sources_v2, "int func_test_member_basetype(struct type_test_member_basetype param, int param_2)\n{\n    return param_2;\n}");
    
    #Member_PointerLevel_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_member_pointerlevel_and_size\n{\n    int i;\n    long long j;\n    double k;\n    struct type_test_member_pointerlevel_and_size* p;\n};");
    @DataDefs_v1 = (@DataDefs_v1, "int func_test_member_pointerlevel_and_size(struct type_test_member_pointerlevel_and_size param, int param_2);");
    @Sources_v1 = (@Sources_v1, "int func_test_member_pointerlevel_and_size(struct type_test_member_pointerlevel_and_size param, int param_2)\n{\n    return param_2;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_member_pointerlevel_and_size\n{\n    int i;\n    long long *j;\n    double k;\n    struct type_test_member_pointerlevel_and_size* p;\n};");
    @DataDefs_v2 = (@DataDefs_v2, "int func_test_member_pointerlevel_and_size(struct type_test_member_pointerlevel_and_size param, int param_2);");
    @Sources_v2 = (@Sources_v2, "int func_test_member_pointerlevel_and_size(struct type_test_member_pointerlevel_and_size param, int param_2)\n{\n    return param_2;\n}");
    
    #Member_PointerLevel
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_member_pointerlevel\n{\n    int i;\n    long *j;\n    double k;\n    struct type_test_member_pointerlevel* p;\n};");
    @DataDefs_v1 = (@DataDefs_v1, "int func_test_member_pointerlevel(struct type_test_member_pointerlevel param, int param_2);");
    @Sources_v1 = (@Sources_v1, "int func_test_member_pointerlevel(struct type_test_member_pointerlevel param, int param_2)\n{\n    return param_2;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_member_pointerlevel\n{\n    int i;\n    long **j;\n    double k;\n    struct type_test_member_pointerlevel* p;\n};");
    @DataDefs_v2 = (@DataDefs_v2, "int func_test_member_pointerlevel(struct type_test_member_pointerlevel param, int param_2);");
    @Sources_v2 = (@Sources_v2, "int func_test_member_pointerlevel(struct type_test_member_pointerlevel param, int param_2)\n{\n    return param_2;\n}");
    
    #Added_Interface
    @DataDefs_v2 = (@DataDefs_v2, "int added_func(int param);");
    @Sources_v2 = (@Sources_v2, "int added_func(int param)\n{\n    return param;\n}");
    
    #Withdrawn_Interface
    @DataDefs_v1 = (@DataDefs_v1, "int withdrawn_func(int param);");
    @Sources_v1 = (@Sources_v1, "int withdrawn_func(int param)\n{\n    return param;\n}");
    
    #Parameter_Type_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "int func_parameter_type_and_size(int param, int other_param);");
    @Sources_v1 = (@Sources_v1, "int func_parameter_type_and_size(int param, int other_param)\n{\n    return other_param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "int func_parameter_type_and_size(long long param, int other_param);");
    @Sources_v2 = (@Sources_v2, "int func_parameter_type_and_size(long long param, int other_param)\n{\n    return other_param;\n}");
    
    #Parameter_Type
    @DataDefs_v1 = (@DataDefs_v1, "int func_parameter_type(int param, int other_param);");
    @Sources_v1 = (@Sources_v1, "int func_parameter_type(int param, int other_param)\n{\n    return other_param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "int func_parameter_type(float param, int other_param);");
    @Sources_v2 = (@Sources_v2, "int func_parameter_type(float param, int other_param)\n{\n    return other_param;\n}");
    
    #Parameter_BaseType (Typedef)
    @DataDefs_v1 = (@DataDefs_v1, "typedef int* PARAM_TYPEDEF;\nint func_parameter_basetypechange_typedef(PARAM_TYPEDEF param);");
    @Sources_v1 = (@Sources_v1, "int func_parameter_basetypechange_typedef(PARAM_TYPEDEF param)\n{\n    return 0;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "typedef const int* PARAM_TYPEDEF;\nint func_parameter_basetypechange_typedef(PARAM_TYPEDEF param);");
    @Sources_v2 = (@Sources_v2, "int func_parameter_basetypechange_typedef(PARAM_TYPEDEF param)\n{\n    return 0;\n}");
    
    #Parameter_BaseType
    @DataDefs_v1 = (@DataDefs_v1, "int func_parameter_basetypechange(int *param);");
    @Sources_v1 = (@Sources_v1, "int func_parameter_basetypechange(int *param)\n{\n    return sizeof(*param);\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "int func_parameter_basetypechange(long long *param);");
    @Sources_v2 = (@Sources_v2, "int func_parameter_basetypechange(long long *param)\n{\n    return sizeof(*param);\n}");
    
    #Parameter_PointerLevel_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "long long func_parameter_pointerlevel_and_size(long long param);");
    @Sources_v1 = (@Sources_v1, "long long func_parameter_pointerlevel_and_size(long long param)\n{\n    return param;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "long long func_parameter_pointerlevel_and_size(long long *param);");
    @Sources_v2 = (@Sources_v2, "long long func_parameter_pointerlevel_and_size(long long *param)\n{\n    return param[5];\n}");
    
    #Parameter_PointerLevel
    @DataDefs_v1 = (@DataDefs_v1, "int func_parameter_pointerlevel(int *param);");
    @Sources_v1 = (@Sources_v1, "int func_parameter_pointerlevel(int *param)\n{\n    return param[5];\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "int func_parameter_pointerlevel(int **param);");
    @Sources_v2 = (@Sources_v2, "int func_parameter_pointerlevel(int **param)\n{\n    return param[5][5];\n}");
    
    #Return_Type_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "int func_return_type_and_size(int param);");
    @Sources_v1 = (@Sources_v1, "int func_return_type_and_size(int param)\n{\n    return 2^(sizeof(int)*8-1)-1;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "long long func_return_type_and_size(int param);");
    @Sources_v2 = (@Sources_v2, "long long func_return_type_and_size(int param)\n{\n    return 2^(sizeof(long long)*8-1)-1;\n}");
    
    #Return_Type
    @DataDefs_v1 = (@DataDefs_v1, "int func_return_type(int param);");
    @Sources_v1 = (@Sources_v1, "int func_return_type(int param)\n{\n    return 2^(sizeof(int)*8-1)-1;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "float func_return_type(int param);");
    @Sources_v2 = (@Sources_v2, "float func_return_type(int param)\n{\n    return 0.7;\n}");
    
    #Return_BaseType
    @DataDefs_v1 = (@DataDefs_v1, "int *func_return_basetypechange(int param);");
    @Sources_v1 = (@Sources_v1, "int *func_return_basetypechange(int param)\n{\n    int *x = (int*)malloc(10*sizeof(int));\n    *x = 2^(sizeof(int)*8-1)-1;\n    return x;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "long long *func_return_basetypechange(int param);");
    @Sources_v2 = (@Sources_v2, "long long *func_return_basetypechange(int param)\n{\n    long long *x = (long long*)malloc(10*sizeof(long long));\n    *x = 2^(sizeof(long long)*8-1)-1;\n    return x;\n}");
    
    #Return_PointerLevel_And_Size
    @DataDefs_v1 = (@DataDefs_v1, "long long func_return_pointerlevel_and_size(int param);");
    @Sources_v1 = (@Sources_v1, "long long func_return_pointerlevel_and_size(int param)\n{\n    return 100;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "long long *func_return_pointerlevel_and_size(int param);");
    @Sources_v2 = (@Sources_v2, "long long *func_return_pointerlevel_and_size(int param)\n{\n    long long *x = (long long*)malloc(10*sizeof(long long));\n    *x = 2^(sizeof(long long)*8-1)-1;\n    return x;\n}");
    
    #Return_PointerLevel
    @DataDefs_v1 = (@DataDefs_v1, "long long *func_return_pointerlevel(int param);");
    @Sources_v1 = (@Sources_v1, "long long *func_return_pointerlevel(int param)\n{\n    long long *x = (long long*)malloc(10*sizeof(long long));\n    *x = 2^(sizeof(long long)*8-1)-1;\n    return x;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "long long **func_return_pointerlevel(int param);");
    @Sources_v2 = (@Sources_v2, "long long **func_return_pointerlevel(int param)\n{\n    long long *x = (long long*)malloc(10*sizeof(long long));\n    *x = 2^(sizeof(long long)*8-1)-1;\n    long *y = (long*)malloc(sizeof(long long));\n    *y=(long)&x;\n    return (long long **)y;\n}");
    
    #typedef to anon struct
    @DataDefs_v1 = (@DataDefs_v1, "
typedef struct
{
    int i;
    long j;
    double k;
} type_test_anon_typedef;
int func_test_anon_typedef(type_test_anon_typedef param);");
    @Sources_v1 = (@Sources_v1, "int func_test_anon_typedef(type_test_anon_typedef param)\n{\n    return 0;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "
typedef struct
{
    int i;
    long j;
    double k;
    union {
        int dummy[256];
        struct {
            char q_skiptable[256];
            const char *p;
            int l;
        } p;
    };
} type_test_anon_typedef;
int func_test_anon_typedef(type_test_anon_typedef param);");
    @Sources_v2 = (@Sources_v2, "int func_test_anon_typedef(type_test_anon_typedef param)\n{\n    return 0;\n}");
    
    #opaque type
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_opaque\n{\n    long long i[5];\n    long j;\n    double k;\n    struct type_test_opaque* p;\n};");
    @DataDefs_v1 = (@DataDefs_v1, "int func_test_opaque(struct type_test_opaque param, int param_2);");
    @Sources_v1 = (@Sources_v1, "int func_test_opaque(struct type_test_opaque param, int param_2)\n{\n    return param_2;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_opaque\n{\n    long long i[5];\n    long long j;\n    double k;\n    struct type_test_opaque* p;\n};");
    @DataDefs_v2 = (@DataDefs_v2, "int func_test_opaque(struct type_test_opaque param, int param_2);");
    @Sources_v2 = (@Sources_v2, "int func_test_opaque(struct type_test_opaque param, int param_2)\n{\n    return param_2;\n}");
    
    #internal function
    @DataDefs_v1 = (@DataDefs_v1, "struct type_test_internal\n{\n    long long i[5];\n    long j;\n    double k;\n    struct type_test_internal* p;\n};");
    @DataDefs_v1 = (@DataDefs_v1, "int func_test_internal(struct type_test_internal param, int param_2);");
    @Sources_v1 = (@Sources_v1, "int func_test_internal(struct type_test_internal param, int param_2)\n{\n    return param_2;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "struct type_test_internal\n{\n    long long i[5];\n    long long j;\n    double k;\n    struct type_test_internal* p;\n};");
    @DataDefs_v2 = (@DataDefs_v2, "int func_test_internal(struct type_test_internal param, int param_2);");
    @Sources_v2 = (@Sources_v2, "int func_test_internal(struct type_test_internal param, int param_2)\n{\n    return param_2;\n}");
    
    #starting versioning
    @DataDefs_v1 = (@DataDefs_v1, "int func_return_type_and_start_versioning(int param);");
    @Sources_v1 = (@Sources_v1, "int func_return_type_and_start_versioning(int param)\n{\n    return 2^(sizeof(int)*8-1)-1;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "int func_return_type_and_start_versioning(int param);");
    @Sources_v2 = (@Sources_v2, "int func_return_type_and_start_versioning(int param)\n{\n    return 2^(sizeof(int)*8-1)-1;\n}\n__asm__(\".symver func_return_type_and_start_versioning,func_return_type_and_start_versioning\@TEST_2.0\");");
    
    #Return_Type and good versioning
    @DataDefs_v1 = (@DataDefs_v1, "int func_return_type_and_good_versioning(int param);");
    @Sources_v1 = (@Sources_v1, "int func_return_type_and_good_versioning(int param)\n{\n    return 2^(sizeof(int)*8-1)-1;\n}\n__asm__(\".symver func_return_type_and_good_versioning,func_return_type_and_good_versioning\@TEST_1.0\");");
    
    @DataDefs_v2 = (@DataDefs_v2, "int func_return_type_and_good_versioning_old(int param);");
    @Sources_v2 = (@Sources_v2, "int func_return_type_and_good_versioning_old(int param)\n{\n    return 2^(sizeof(int)*8-1)-1;\n}\n__asm__(\".symver func_return_type_and_good_versioning_old,func_return_type_and_good_versioning\@TEST_1.0\");");
    
    @DataDefs_v2 = (@DataDefs_v2, "float func_return_type_and_good_versioning(int param);");
    @Sources_v2 = (@Sources_v2, "float func_return_type_and_good_versioning(int param)\n{\n    return 0.7;\n}\n__asm__(\".symver func_return_type_and_good_versioning,func_return_type_and_good_versioning\@TEST_2.0\");");
    
    #Return_Type and bad versioning
    @DataDefs_v1 = (@DataDefs_v1, "int func_return_type_and_bad_versioning(int param);");
    @Sources_v1 = (@Sources_v1, "int func_return_type_and_bad_versioning(int param)\n{\n    return 2^(sizeof(int)*8-1)-1;\n}\n__asm__(\".symver func_return_type_and_bad_versioning,func_return_type_and_bad_versioning\@TEST_1.0\");");
    
    @DataDefs_v2 = (@DataDefs_v2, "float func_return_type_and_bad_versioning_old(int param);");
    @Sources_v2 = (@Sources_v2, "float func_return_type_and_bad_versioning_old(int param)\n{\n    return 0.7;\n}\n__asm__(\".symver func_return_type_and_bad_versioning_old,func_return_type_and_bad_versioning\@TEST_1.0\");");
    
    @DataDefs_v2 = (@DataDefs_v2, "float func_return_type_and_bad_versioning(int param);");
    @Sources_v2 = (@Sources_v2, "float func_return_type_and_bad_versioning(int param)\n{\n    return 0.7;\n}\n__asm__(\".symver func_return_type_and_bad_versioning,func_return_type_and_bad_versioning\@TEST_2.0\");");
    
    #unnamed struct/union fields within structs/unions
    @DataDefs_v1 = (@DataDefs_v1, "
typedef struct {
  int a;
  union {
    int b;
    float c;
  };
  int d;
} type_test_unnamed;
int func_test_unnamed(type_test_unnamed param);");
    @Sources_v1 = (@Sources_v1, "int func_test_unnamed(type_test_unnamed param)\n{\n    return 0;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "
typedef struct {
  int a;
  union {
    long double b;
    float c;
  };
  int d;
} type_test_unnamed;
int func_test_unnamed(type_test_unnamed param);");
    @Sources_v2 = (@Sources_v2, "int func_test_unnamed(type_test_unnamed param)\n{\n    return 0;\n}");
    
    #constants
    @DataDefs_v1 = (@DataDefs_v1, "#define TEST_PUBLIC_CONSTANT \"old_value\"");
    @DataDefs_v2 = (@DataDefs_v2, "#define TEST_PUBLIC_CONSTANT \"new_value\"");
    
    @DataDefs_v1 = (@DataDefs_v1, "#define TEST_PRIVATE_CONSTANT \"old_value\"\n#undef TEST_PRIVATE_CONSTANT");
    @DataDefs_v2 = (@DataDefs_v2, "#define TEST_PRIVATE_CONSTANT \"new_value\"\n#undef TEST_PRIVATE_CONSTANT");
    
    #anon ptr typedef
    @DataDefs_v1 = (@DataDefs_v1, "
#ifdef __cplusplus
extern \"C\" {
#endif
typedef struct {
  int a;
} *type_test_anonptr_typedef;
extern __attribute__ ((visibility(\"default\"))) int func_test_anonptr_typedef(type_test_anonptr_typedef param);
#ifdef __cplusplus
}
#endif");
    @Sources_v1 = (@Sources_v1, "__attribute__ ((visibility(\"default\"))) int func_test_anonptr_typedef(type_test_anonptr_typedef param)\n{\n    return 0;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "
#ifdef __cplusplus
extern \"C\" {
#endif
typedef struct {
  float a;
} *type_test_anonptr_typedef;
extern __attribute__ ((visibility(\"default\"))) int func_test_anonptr_typedef(type_test_anonptr_typedef param);
#ifdef __cplusplus
}
#endif");
    @Sources_v2 = (@Sources_v2, "__attribute__ ((visibility(\"default\"))) int func_test_anonptr_typedef(type_test_anonptr_typedef param)\n{\n    return 0;\n}");
    
    #typedefs
    @DataDefs_v1 = (@DataDefs_v1, "typedef float TYPEDEF_TYPE;\nint func_parameter_typedef_change(TYPEDEF_TYPE param);");
    @Sources_v1 = (@Sources_v1, "int func_parameter_typedef_change(TYPEDEF_TYPE param)\n{\n    return 1.0;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "typedef int TYPEDEF_TYPE;\nint func_parameter_typedef_change(TYPEDEF_TYPE param);");
    @Sources_v2 = (@Sources_v2, "int func_parameter_typedef_change(TYPEDEF_TYPE param)\n{\n    return 1;\n}");
    
    #typedefs in member type
    @DataDefs_v1 = (@DataDefs_v1, "typedef float TYPEDEF_TYPE_2;\nstruct type_test_member_typedef_change{\nTYPEDEF_TYPE_2 m;};\nint func_test_member_typedef_change(struct type_test_member_typedef_change param);");
    @Sources_v1 = (@Sources_v1, "int func_test_member_typedef_change(struct type_test_member_typedef_change param)\n{\n    return 1;\n}");
    
    @DataDefs_v2 = (@DataDefs_v2, "typedef int TYPEDEF_TYPE_2;\nstruct type_test_member_typedef_change{\nTYPEDEF_TYPE_2 m;};\nint func_test_member_typedef_change(struct type_test_member_typedef_change param);");
    @Sources_v2 = (@Sources_v2, "int func_test_member_typedef_change(struct type_test_member_typedef_change param)\n{\n    return 1;\n}");
    
    create_TestSuite("abi_changes_test_c", "C", join("\n\n", @DataDefs_v1), join("\n\n", @Sources_v1), join("\n\n", @DataDefs_v2), join("\n\n", @Sources_v2), "type_test_opaque", "func_test_internal");
}

sub create_TestSuite($$$$$$$$)
{
    my ($Dir, $Lang, $DataDefs_v1, $Sources_v1, $DataDefs_v2, $Sources_v2, $Opaque, $Private) = @_;
    my $Ext = ($Lang eq "C++")?"cpp":"c";
    my $Gcc = ($Lang eq "C++")?"g++":"gcc";
    #creating test suite
    my $Path_v1 = "$Dir/lib_abi_changes_test.v1";
    my $Path_v2 = "$Dir/lib_abi_changes_test.v2";
    
    system("rm", "-fr", $Path_v1, $Path_v2);
    system("mkdir", "-p", $Path_v1, $Path_v2);
    
    writeFile("$Path_v1/version", "TEST_1.0 {\n};\nTEST_2.0 {\n};\n");
    writeFile("$Path_v1/lib_abi_changes_test.h", "#include <stdlib.h>\n".$DataDefs_v1."\n");
    writeFile("$Path_v1/lib_abi_changes_test.$Ext", "#include \"lib_abi_changes_test.h\"\n".$Sources_v1."\n");
    writeFile("$Dir/descriptor.v1", "<version>\n    1.0.0\n</version>\n\n<headers>\n    ".$ENV{"PWD"}."/$Path_v1/\n</headers>\n\n<libs>\n    ".$ENV{"PWD"}."/$Path_v1/\n</libs>\n\n<opaque_types>\n    $Opaque\n</opaque_types>\n\n<skip_interfaces>\n    $Private\n</skip_interfaces>\n\n<include_paths>\n    ".$ENV{"PWD"}."/$Path_v1\n</include_paths>\n");
    
    writeFile("$Path_v2/version", "TEST_1.0 {\n};\nTEST_2.0 {\n};\n");
    writeFile("$Path_v2/lib_abi_changes_test.h", "#include <stdlib.h>\n".$DataDefs_v2."\n");
    writeFile("$Path_v2/lib_abi_changes_test.$Ext", "#include \"lib_abi_changes_test.h\"\n".$Sources_v2."\n");
    writeFile("$Dir/descriptor.v2", "<version>\n    2.0.0\n</version>\n\n<headers>\n    ".$ENV{"PWD"}."/$Path_v2/\n</headers>\n\n<libs>\n    ".$ENV{"PWD"}."/$Path_v2/\n</libs>\n\n<opaque_types>\n    $Opaque\n</opaque_types>\n\n<skip_interfaces>\n    $Private\n</skip_interfaces>\n\n<include_paths>\n    ".$ENV{"PWD"}."/$Path_v2\n</include_paths>\n");
    
    system("$Gcc $Path_v1/lib_abi_changes_test.h");
    if($?)
    {
        print "ERROR: can't compile \'$Path_v1/lib_abi_changes_test.h\'\n";
        return;
    }
    system("$Gcc -Wl,--version-script $Path_v1/version -shared $Path_v1/lib_abi_changes_test.$Ext -o $Path_v1/lib_abi_changes_test.so");
    if($?)
    {
        print "ERROR: can't compile \'$Path_v1/lib_abi_changes_test.$Ext\'\n";
        return;
    }
    system("$Gcc $Path_v2/lib_abi_changes_test.h");
    if($?)
    {
        print "ERROR: can't compile \'$Path_v2/lib_abi_changes_test.h\'\n";
        return;
    }
    system("$Gcc -Wl,--version-script $Path_v2/version -shared $Path_v2/lib_abi_changes_test.$Ext -o $Path_v2/lib_abi_changes_test.so");
    if($?)
    {
        print "ERROR: can't compile \'$Path_v2/lib_abi_changes_test.$Ext\'\n";
        return;
    }
    
    #running abi-compliance-checker
    system("$0 -l lib_$Dir -d1 $Dir/descriptor.v1 -d2 $Dir/descriptor.v2");
}

sub writeFile($$)
{
    my ($Path, $Content) = @_;
    return if(not $Path);
    open (FILE, ">".$Path) || die ("Can't open file $Path\n");
    print FILE $Content;
    close(FILE);
}

sub readFile($)
{
    my $Path = $_[0];
    return "" if(not $Path or not -f $Path);
    open (FILE, $Path);
    my $Content = join("\n", <FILE>);
    close(FILE);
    return toUnix($Content);
}

sub toUnix($)
{
    my $Text = $_[0];
    $Text=~s/\r//g;
    return $Text;
}

sub getArch()
{
    my $Arch = $ENV{"CPU"};
    if(not $Arch)
    {
        $Arch = `uname -m`;
        chomp($Arch);
    }
    if(not $Arch)
    {
        $Arch = `uname -p`;
        chomp($Arch);
    }
    $Arch = "x86" if($Arch =~ /i[3-7]86/);
    return $Arch;
}

sub get_Report_Header()
{
    my $Report_Header = "<h1>ABI compliance report for the library <span style='color:Blue;white-space:nowrap;'>$TargetLibraryName </span> from version <span style='color:Red;white-space:nowrap;'>".$Descriptor{1}{"Version"}."</span> to <span style='color:Red;white-space:nowrap;'>".$Descriptor{2}{"Version"}."</span> on <span style='color:Blue;'>".getArch()."</span> ".(($AppPath)?"relating to the portability of application <span style='color:Blue;'>".get_FileName($AppPath)."</span>":"")."</h1>\n";
    return "<!--Header-->\n".$Report_Header."<!--Header_End-->\n";
}

sub get_SourceInfo()
{
    my $CheckedHeaders = "<!--Checked_Headers-->\n<a name='Checked_Headers'></a><h2 style='margin-bottom:0px;padding-bottom:0px;'>Checked header files (".keys(%{$Headers{1}}).")</h2><hr/>\n";
    foreach my $Header_Dest (sort {lc($Headers{1}{$a}{"Name"}) cmp lc($Headers{1}{$b}{"Name"})} keys(%{$Headers{1}}))
    {
        my $Header_Name = $Headers{1}{$Header_Dest}{"Name"};
        my $Dest_Count = keys(%{$HeaderName_Destinations{1}{$Header_Name}});
        my $Identity = $Headers{1}{$Header_Dest}{"Identity"};
        my $Dest_Comment = ($Dest_Count>1 and $Identity=~/\//)?" ($Identity)":"";
        $CheckedHeaders .= "<span class='header_list_elem'>$Header_Name"."$Dest_Comment</span><br/>\n";
    }
    $CheckedHeaders .= "<!--Checked_Headers_End--><br/><a style='font-size:11px;' href='#Top'>to the top</a><br/>\n";
    my $CheckedLibs = "<!--Checked_Libs-->\n<a name='Checked_Libs'></a><h2 style='margin-bottom:0px;padding-bottom:0px;'>Checked shared objects (".keys(%{$SoNames_All{1}}).")</h2><hr/>\n";
    foreach my $Library (sort {lc($a) cmp lc($b)}  keys(%{$SoNames_All{1}}))
    {
        $CheckedLibs .= "<span class='solib_list_elem'>$Library</span><br/>\n";
    }
    $CheckedLibs .= "<!--Checked_Libs_End--><br/><a style='font-size:11px;' href='#Top'>to the top</a><br/>\n";
    return $CheckedHeaders.$CheckedLibs;
}

sub get_TypeProblems_Count($$)
{
    my ($TypeChanges, $TargetPriority) = @_;
    my $Type_Problems_Count = 0;
    foreach my $TypeName (sort keys(%{$TypeChanges}))
    {
        my %Kinds_Target = ();
        foreach my $Kind (keys(%{$TypeChanges->{$TypeName}}))
        {
            foreach my $Location (keys(%{$TypeChanges->{$TypeName}{$Kind}}))
            {
                my $Priority = $TypeChanges->{$TypeName}{$Kind}{$Location}{"Priority"};
                next if($Priority ne $TargetPriority);
                my $Target = $TypeChanges->{$TypeName}{$Kind}{$Location}{"Target"};
                next if($Kinds_Target{$Kind}{$Target});
                $Kinds_Target{$Kind}{$Target} = 1;
                $Type_Problems_Count += 1;
            }
        }
    }
    return $Type_Problems_Count;
}

sub get_Summary()
{
    my ($Added, $Withdrawn, $I_Problems_High, $I_Problems_Medium, $I_Problems_Low, $T_Problems_High, $T_Problems_Medium, $T_Problems_Low) = (0,0,0,0,0,0,0,0);
    foreach my $Interface (sort keys(%CompatProblems))
    {
        foreach my $Kind (sort keys(%{$CompatProblems{$Interface}}))
        {
            if($InterfaceProblems_Kind{$Kind})
            {
                foreach my $Location (sort keys(%{$CompatProblems{$Interface}{$Kind}}))
                {
                    if($Kind eq "Added_Interface")
                    {
                        $Added += 1;
                    }
                    elsif($Kind eq "Withdrawn_Interface")
                    {
                        $Withdrawn += 1;
                    }
                    else
                    {
                        if($CompatProblems{$Interface}{$Kind}{$Location}{"Priority"} eq "High")
                        {
                            $I_Problems_High += 1;
                        }
                        elsif($CompatProblems{$Interface}{$Kind}{$Location}{"Priority"} eq "Medium")
                        {
                            $I_Problems_Medium += 1;
                        }
                        elsif($CompatProblems{$Interface}{$Kind}{$Location}{"Priority"} eq "Low")
                        {
                            $I_Problems_Low += 1;
                        }
                    }
                }
            }
        }
    }
    
    my (%TypeChanges, %Type_MaxPriority);
    foreach my $Interface (sort keys(%CompatProblems))
    {
        foreach my $Kind (keys(%{$CompatProblems{$Interface}}))
        {
            if($TypeProblems_Kind{$Kind})
            {
                foreach my $Location (keys(%{$CompatProblems{$Interface}{$Kind}}))
                {
                    my $Type_Name = $CompatProblems{$Interface}{$Kind}{$Location}{"Type_Name"};
                    my $Priority = $CompatProblems{$Interface}{$Kind}{$Location}{"Priority"};
                    %{$TypeChanges{$Type_Name}{$Kind}{$Location}} = %{$CompatProblems{$Interface}{$Kind}{$Location}};
                    $Type_MaxPriority{$Type_Name}{$Kind} = max_priority($Type_MaxPriority{$Type_Name}{$Kind}, $Priority);
                }
            }
        }
    }
    
    foreach my $Type_Name (keys(%TypeChanges))
    {
        foreach my $Kind (keys(%{$TypeChanges{$Type_Name}}))
        {
            foreach my $Location (keys(%{$TypeChanges{$Type_Name}{$Kind}}))
            {
                my $Priority = $TypeChanges{$Type_Name}{$Kind}{$Location}{"Priority"};
                if(cmp_priority($Type_MaxPriority{$Type_Name}{$Kind}, $Priority))
                {
                    delete($TypeChanges{$Type_Name}{$Kind}{$Location});
                }
            }
        }
    }
    
    $T_Problems_High = get_TypeProblems_Count(\%TypeChanges, "High");
    $T_Problems_Medium = get_TypeProblems_Count(\%TypeChanges, "Medium");
    $T_Problems_Low = get_TypeProblems_Count(\%TypeChanges, "Low");
    
    #summary
    my $Summary = "<h2 style='margin-bottom:0px;padding-bottom:0px;'>Summary</h2><hr/>";
    $Summary .= "<table cellpadding='3' border='1' style='border-collapse:collapse;'>";
    
    
    my $Checked_Headers_Link = "0";
    $Checked_Headers_Link = "<a href='#Checked_Headers' style='color:Blue;'>".keys(%{$Headers{1}})."</a>" if(keys(%{$Headers{1}})>0);
    $Summary .= "<tr><td class='table_header summary_item'>Total header files checked</td><td class='summary_item_value'>$Checked_Headers_Link</td></tr>";
    
    my $Checked_Libs_Link = "0";
    $Checked_Libs_Link = "<a href='#Checked_Libs' style='color:Blue;'>".keys(%{$SoNames_All{1}})."</a>" if(keys(%{$SoNames_All{1}})>0);
    $Summary .= "<tr><td class='table_header summary_item'>Total shared objects checked</td><td class='summary_item_value'>$Checked_Libs_Link</td></tr>";
    $Summary .= "<tr><td class='table_header summary_item'>Total interfaces / types checked</td><td class='summary_item_value'>".keys(%CheckedInterfaces)." / ".keys(%CheckedTypes)."</td></tr>";
    
    my $Verdict = "<span style='color:Green;'><b>Compatible</b></span>";
    $Verdict = "<span style='color:Red;'><b>Incompatible</b></span>" if(($Withdrawn>0) or ($I_Problems_High>0) or ($T_Problems_High>0));
    $Summary .= "<tr><td class='table_header summary_item'>Verdict</td><td class='summary_item_value'>$Verdict</td></tr>";
    
    $Summary .= "</table>\n";
    
    #problem summary
    my $Problem_Summary = "<h2 style='margin-bottom:0px;padding-bottom:0px;'>Problem Summary</h2><hr/>";
    $Problem_Summary .= "<table cellpadding='3' border='1' style='border-collapse:collapse;'>";
    
    my $Added_Link = "0";
    $Added_Link = "<a href='#Added' style='color:Blue;'>$Added</a>" if($Added>0);
    $Problem_Summary .= "<tr><td class='table_header summary_item' colspan='2'>Added interfaces</td><td class='summary_item_value'>$Added_Link</td></tr>";
    
    my $WIthdrawn_Link = "0";
    $WIthdrawn_Link = "<a href='#Withdrawn' style='color:Blue;'>$Withdrawn</a>" if($Withdrawn>0);
    $Problem_Summary .= "<tr><td class='table_header summary_item' colspan='2'>Withdrawn interfaces</td><td class='summary_item_value'>$WIthdrawn_Link</td></tr>";
    
    my $TH_Link = "0";
    $TH_Link = "<a href='#Type_Problems_High' style='color:Blue;'>$T_Problems_High</a>" if($T_Problems_High>0);
    $Problem_Summary .= "<tr><td class='table_header summary_item' rowspan='3'>Problems in<br/>Data Types</td><td class='table_header summary_item' style='color:Red;'>High risk</td><td align='right' class='summary_item_value'>$TH_Link</td></tr>";
    
    my $TM_Link = "0";
    $TM_Link = "<a href='#Type_Problems_Medium' style='color:Blue;'>$T_Problems_Medium</a>" if($T_Problems_Medium>0);
    $Problem_Summary .= "<tr><td class='table_header summary_item'>Medium risk</td><td class='summary_item_value'>$TM_Link</td></tr>";
    
    my $TL_Link = "0";
    $TL_Link = "<a href='#Type_Problems_Low' style='color:Blue;'>$T_Problems_Low</a>" if($T_Problems_Low>0);
    $Problem_Summary .= "<tr><td class='table_header summary_item'>Low risk</td><td class='summary_item_value'>$TL_Link</td></tr>";
    
    my $IH_Link = "0";
    $IH_Link = "<a href='#Interface_Problems_High' style='color:Blue;'>$I_Problems_High</a>" if($I_Problems_High>0);
    $Problem_Summary .= "<tr><td class='table_header summary_item' rowspan='3'>Interface<br/>problems</td><td class='table_header summary_item' style='color:Red;'>High risk</td><td class='summary_item_value'>$IH_Link</td></tr>";
    
    my $IM_Link = "0";
    $IM_Link = "<a href='#Interface_Problems_Medium' style='color:Blue;'>$I_Problems_Medium</a>" if($I_Problems_Medium>0);
    $Problem_Summary .= "<tr><td class='table_header summary_item'>Medium risk</td><td class='summary_item_value'>$IM_Link</td></tr>";
    
    my $IL_Link = "0";
    $IL_Link = "<a href='#Interface_Problems_Low' style='color:Blue;'>$I_Problems_Low</a>" if($I_Problems_Low>0);
    $Problem_Summary .= "<tr><td class='table_header summary_item'>Low risk</td><td class='summary_item_value'>$IL_Link</td></tr>";
    
    my $ChangedConstants_Link = "0";
    $ChangedConstants_Link = "<a href='#Constants' style='color:Blue;'>".keys(%ConstantProblems)."</a>" if(keys(%ConstantProblems)>0);
    $Problem_Summary .= "<tr><td class='table_header summary_item' colspan='2'>Constants Problems</td><td class='summary_item_value'>$ChangedConstants_Link</td></tr>";
    
    $Problem_Summary .= "</table>\n";
    return "<!--Summary-->\n".$Summary.$Problem_Summary."<!--Summary_End-->\n";
}

sub get_Report_ChangedConstants()
{
    my ($CHANGED_CONSTANTS, %HeaderConstant);
    foreach my $Name (keys(%ConstantProblems))
    {
        $HeaderConstant{$ConstantProblems{$Name}{"Header"}}{$Name} = 1;
    }
    my $Constants_Number = 0;
    foreach my $HeaderName (sort {lc($a) cmp lc($b)} keys(%HeaderConstant))
    {
        $CHANGED_CONSTANTS .= "<span class='header_name'>$HeaderName</span><br/>\n";
        foreach my $Name (sort {lc($a) cmp lc($b)} keys(%{$HeaderConstant{$HeaderName}}))
        {
            $Constants_Number += 1;
            my $Old_Value = htmlSpecChars($ConstantProblems{$Name}{"Old_Value"});
            my $New_Value = htmlSpecChars($ConstantProblems{$Name}{"New_Value"});
            my $Incompatibility = "The value of constant <b>$Name</b> has been changed from <span style='white-space:nowrap;'><b>$Old_Value</b></span> to <span style='white-space:nowrap;'><b>$New_Value</b></span>.";
            my $Effect = "If application uses this constant as a parameter of some interface than its execution may change.";
            my $ConstantProblemsReport = "<tr><td align='center' valign='top' class='table_header'><span class='problem_num'>1</span></td><td align='left' valign='top'><span class='problem_body'>".$Incompatibility."</span></td><td align='left' valign='top'><span class='problem_body'>$Effect</span></td></tr>\n";
            $CHANGED_CONSTANTS .= $ContentSpanStart."<span class='extension'>[+]</span> ".$Name.$ContentSpanEnd."<br/>\n$ContentDivStart<table width='900px' cellpadding='3' cellspacing='0' class='problems_table'><tr><td align='center' width='2%' class='table_header'><span class='problem_title' style='white-space:nowrap;'></span></td><td width='47%' align='center' class='table_header'><span class='problem_sub_title'>Incompatibility</span></td><td align='center' class='table_header'><span class='problem_sub_title'>Effect</span></td></tr>$ConstantProblemsReport</table><br/>$ContentDivEnd\n";
            $CHANGED_CONSTANTS = insertIDs($CHANGED_CONSTANTS);
        }
        $CHANGED_CONSTANTS .= "<br/>\n";
    }
    if($CHANGED_CONSTANTS)
    {
        $CHANGED_CONSTANTS = "<a name='Constants'></a><h2 style='margin-bottom:0px;padding-bottom:0px;'>Constants Problems ($Constants_Number)</h2><hr/>\n"."<!--Changed_Constants-->\n".$CHANGED_CONSTANTS."<!--Changed_Constants_End-->\n"."<input id='Changed_Constants_Count' type='hidden' value=\'$Constants_Number\' /><a style='font-size:11px;' href='#Top'>to the top</a><br/>\n";
    }
    return $CHANGED_CONSTANTS;
}

sub get_Report_Added()
{
    my $ADDED_INTERFACES;
    #added interfaces
    my %FuncAddedInHeaderLib;
    foreach my $Interface (sort keys(%CompatProblems))
    {
        foreach my $Kind (sort keys(%{$CompatProblems{$Interface}}))
        {
            foreach my $Location (sort keys(%{$CompatProblems{$Interface}{$Kind}}))
            {
                if($Kind eq "Added_Interface")
                {
                    $FuncAddedInHeaderLib{$CompatProblems{$Interface}{$Kind}{$Location}{"Header"}}{$CompatProblems{$Interface}{$Kind}{$Location}{"New_SoName"}}{$Interface} = 1;
                    last;
                }
            }
        }
    }
    my $Added_Number = 0;
    foreach my $HeaderName (sort {lc($a) cmp lc($b)} keys(%FuncAddedInHeaderLib))
    {
        foreach my $SoName (sort {lc($a) cmp lc($b)} keys(%{$FuncAddedInHeaderLib{$HeaderName}}))
        {
            if($HeaderName)
            {
                $ADDED_INTERFACES .= "<span class='header_name'>$HeaderName</span>, <span class='solib_name'>$SoName</span><br/>\n";
            }
            else
            {
                $ADDED_INTERFACES .= "<span class='solib_name'>$SoName</span><br/>\n";
            }
            foreach my $Interface (sort {lc($CompatProblems{$a}{"Added_Interface"}{"SharedLibrary"}{"Signature"}) cmp lc($CompatProblems{$b}{"Added_Interface"}{"SharedLibrary"}{"Signature"})} keys(%{$FuncAddedInHeaderLib{$HeaderName}{$SoName}}))
            {
                $Added_Number += 1;
                my $SubReport = "";
                my $Signature = $CompatProblems{$Interface}{"Added_Interface"}{"SharedLibrary"}{"Signature"};
                if($Interface =~ /\A_Z/)
                {
                    if($Signature)
                    {
                        $SubReport = insertIDs($ContentSpanStart.highLight_Signature_Italic_Color(htmlSpecChars($Signature)).$ContentSpanEnd."<br/>\n".$ContentDivStart."<span class='mangled'>[ symbol: <b>$Interface</b> ]</span><br/><br/>".$ContentDivEnd."\n");
                    }
                    else
                    {
                        $SubReport = "<span class=\"interface_name\">".$Interface."</span>"."<br/>\n";
                    }
                }
                else
                {
                    if($Signature)
                    {
                        $SubReport = "<span class=\"interface_name\">".highLight_Signature_Italic_Color($Signature)."</span>"."<br/>\n";
                    }
                    else
                    {
                        $SubReport = "<span class=\"interface_name\">".$Interface."</span>"."<br/>\n";
                    }
                }
                $ADDED_INTERFACES .= $SubReport;
            }
            $ADDED_INTERFACES .= "<br/>\n";
        }
    }
    if($ADDED_INTERFACES)
    {
        $ADDED_INTERFACES = "<a name='Added'></a><h2 style='margin-bottom:0px;padding-bottom:0px;'>Added Interfaces ($Added_Number)</h2><hr/>\n"."<!--Added_Interfaces-->\n".$ADDED_INTERFACES."<!--Added_Interfaces_End-->\n"."<input id='Added_Interfaces_Count' type='hidden' value=\'$Added_Number\' /><a style='font-size:11px;' href='#Top'>to the top</a><br/>\n";
    }
    return $ADDED_INTERFACES;
}

sub get_Report_Withdrawn()
{
    my $WITHDRAWN_INTERFACES;
    #withdrawn interfaces
    my %FuncWithdrawnFromHeaderLib;
    foreach my $Interface (sort keys(%CompatProblems))
    {
        foreach my $Kind (sort keys(%{$CompatProblems{$Interface}}))
        {
            foreach my $Location (sort keys(%{$CompatProblems{$Interface}{$Kind}}))
            {
                if($Kind eq "Withdrawn_Interface")
                {
                    $FuncWithdrawnFromHeaderLib{$CompatProblems{$Interface}{$Kind}{$Location}{"Header"}}{$CompatProblems{$Interface}{$Kind}{$Location}{"Old_SoName"}}{$Interface} = 1;
                    last;
                }
            }
        }
    }
    my $Withdrawn_Number = 0;
    foreach my $HeaderName (sort {lc($a) cmp lc($b)} keys(%FuncWithdrawnFromHeaderLib))
    {
        foreach my $SoName (sort {lc($a) cmp lc($b)} keys(%{$FuncWithdrawnFromHeaderLib{$HeaderName}}))
        {
            if($HeaderName)
            {
                $WITHDRAWN_INTERFACES .= "<span class='header_name'>$HeaderName</span>, <span class='solib_name'>$SoName</span><br/>\n";
            }
            else
            {
                $WITHDRAWN_INTERFACES .= "<span class='solib_name'>$SoName</span><br/>\n";
            }
            foreach my $Interface (sort {lc($CompatProblems{$a}{"Withdrawn_Interface"}{"SharedLibrary"}{"Signature"}) cmp lc($CompatProblems{$b}{"Withdrawn_Interface"}{"SharedLibrary"}{"Signature"})} keys(%{$FuncWithdrawnFromHeaderLib{$HeaderName}{$SoName}}))
            {
                $Withdrawn_Number += 1;
                my $SubReport = "";
                my $Signature = $CompatProblems{$Interface}{"Withdrawn_Interface"}{"SharedLibrary"}{"Signature"};
                if($Interface =~ /\A_Z/)
                {
                    if($Signature)
                    {
                        $SubReport = insertIDs($ContentSpanStart.highLight_Signature_Italic_Color(htmlSpecChars($Signature)).$ContentSpanEnd."<br/>\n".$ContentDivStart."<span class='mangled'>[ symbol: <b>$Interface</b> ]</span><br/><br/>".$ContentDivEnd."\n");
                    }
                    else
                    {
                        $SubReport = "<span class=\"interface_name\">".$Interface."</span>"."<br/>\n";
                    }
                }
                else
                {
                    if($Signature)
                    {
                        $SubReport = "<span class=\"interface_name\">".highLight_Signature_Italic_Color($Signature)."</span>"."<br/>\n";
                    }
                    else
                    {
                        $SubReport = "<span class=\"interface_name\">".$Interface."</span>"."<br/>\n";
                    }
                }
                $WITHDRAWN_INTERFACES .= $SubReport;
            }
            $WITHDRAWN_INTERFACES .= "<br/>\n";
        }
    }
    if($WITHDRAWN_INTERFACES)
    {
        $WITHDRAWN_INTERFACES = "<a name='Withdrawn'></a><h2 style='margin-bottom:0px;padding-bottom:0px;'>Withdrawn Interfaces ($Withdrawn_Number)</h2><hr/>\n"."<!--Withdrawn_Interfaces-->\n".$WITHDRAWN_INTERFACES."<!--Withdrawn_Interfaces_End-->\n"."<input id='Withdrawn_Interfaces_Count' type='hidden' value=\'$Withdrawn_Number\' /><a style='font-size:11px;' href='#Top'>to the top</a><br/>\n";
    }
    return $WITHDRAWN_INTERFACES;
}

sub get_Report_InterfaceProblems($)
{
    my $TargetPriority = $_[0];
    my ($INTERFACE_PROBLEMS, %FuncHeaderLib);
    foreach my $Interface (sort keys(%CompatProblems))
    {
        foreach my $Kind (sort keys(%{$CompatProblems{$Interface}}))
        {
            if($InterfaceProblems_Kind{$Kind} and ($Kind ne "Added_Interface") and ($Kind ne "Withdrawn_Interface"))
            {
                foreach my $Location (sort keys(%{$CompatProblems{$Interface}{$Kind}}))
                {
                    my $SoName = $CompatProblems{$Interface}{$Kind}{$Location}{"Old_SoName"};
                    my $HeaderName = $CompatProblems{$Interface}{$Kind}{$Location}{"Header"};
                    $FuncHeaderLib{$HeaderName}{$SoName}{$Interface} = 1;
                    last;
                }
            }
        }
    }
    my $Problems_Number = 0;
    #interface problems
    foreach my $HeaderName (sort {lc($a) cmp lc($b)} keys(%FuncHeaderLib))
    {
        foreach my $SoName (sort {lc($a) cmp lc($b)} keys(%{$FuncHeaderLib{$HeaderName}}))
        {
            my $HEADER_LIB_REPORT = "";
            foreach my $Interface (sort {lc($tr_name{$a}) cmp lc($tr_name{$b})} keys(%{$FuncHeaderLib{$HeaderName}{$SoName}}))
            {
                my $Signature = "";
                my $InterfaceProblemsReport = "";
                my $ProblemNum = 1;
                foreach my $Kind (keys(%{$CompatProblems{$Interface}}))
                {
                    foreach my $Location (keys(%{$CompatProblems{$Interface}{$Kind}}))
                    {
                        my $Incompatibility = "";
                        my $Effect = "";
                        my $Old_Value = htmlSpecChars($CompatProblems{$Interface}{$Kind}{$Location}{"Old_Value"});
                        my $New_Value = htmlSpecChars($CompatProblems{$Interface}{$Kind}{$Location}{"New_Value"});
                        my $Priority = $CompatProblems{$Interface}{$Kind}{$Location}{"Priority"};
                        my $Target = $CompatProblems{$Interface}{$Kind}{$Location}{"Target"};
                        my $Old_Size = $CompatProblems{$Interface}{$Kind}{$Location}{"Old_Size"};
                        my $New_Size = $CompatProblems{$Interface}{$Kind}{$Location}{"New_Size"};
                        my $InitialType_Type = $CompatProblems{$Interface}{$Kind}{$Location}{"InitialType_Type"};
                        my $Parameter_Position = $CompatProblems{$Interface}{$Kind}{$Location}{"Parameter_Position"};
                        my $Parameter_Position_Str = num_to_str($Parameter_Position + 1);
                        $Signature = $CompatProblems{$Interface}{$Kind}{$Location}{"Signature"} if(not $Signature);
                        next if($Priority ne $TargetPriority);
                        if($Kind eq "Function_Become_Static")
                        {
                            $Incompatibility = "Function become <b>static</b>.\n";
                            $Effect = "Layout of parameter's stack has been changed and therefore parameters in higher positions in the stack may be incorrectly initialized by applications.";
                        }
                        elsif($Kind eq "Function_Become_NonStatic")
                        {
                            $Incompatibility = "Function become <b>non-static</b>.\n";
                            $Effect = "Layout of parameter's stack has been changed and therefore parameters in higher positions in the stack may be incorrectly initialized by applications.";
                        }
                        elsif($Kind eq "Parameter_Type_And_Size")
                        {
                            $Incompatibility = "Type of $Parameter_Position_Str parameter <b>$Target</b> has been changed from <span style='white-space:nowrap;'><b>$Old_Value</b> (<b>$Old_Size</b> bytes)</span> to <span style='white-space:nowrap;'><b>$New_Value</b> (<b>$New_Size</b> bytes)</span>.\n";
                            $Effect = "Layout of parameter's stack has been changed and therefore parameters in higher positions in the stack may be incorrectly initialized by applications.";
                        }
                        elsif($Kind eq "Parameter_Type")
                        {
                            $Incompatibility = "Type of $Parameter_Position_Str parameter <b>$Target</b> has been changed from <span style='white-space:nowrap;'><b>$Old_Value</b></span> to <span style='white-space:nowrap;'><b>$New_Value</b></span>.\n";
                            $Effect = "Replacement of parameter data type may indicate a change in the semantic meaning of this parameter.";
                        }
                        elsif($Kind eq "Withdrawn_Parameter")
                        {
                            $Incompatibility = "$Parameter_Position_Str parameter <b>$Target</b> has been withdrawn from the interface signature.\n";
                            $Effect = "This parameter will be ignored by the interface.";
                        }
                        elsif($Kind eq "Added_Parameter")
                        {
                            $Incompatibility = "$Parameter_Position_Str parameter <b>$Target</b> has been added to the interface signature.\n";
                            $Effect = "This parameter will not be initialized by applications.";
                        }
                        elsif($Kind eq "Parameter_BaseType_And_Size")
                        {
                            if($InitialType_Type eq "Pointer")
                            {
                                $Incompatibility = "Base type of $Parameter_Position_Str parameter <b>$Target</b> (pointer) has been changed from <span style='white-space:nowrap;'><b>$Old_Value</b> (<b>$Old_Size</b> bytes)</span> to <span style='white-space:nowrap;'><b>$New_Value</b> (<b>$New_Size</b> bytes)</span>.\n";
                                $Effect = "Memory stored by pointer may be incorrectly initialized by applications.";
                            }
                            else
                            {
                                $Incompatibility = "Base type of $Parameter_Position_Str parameter <b>$Target</b> has been changed from <span style='white-space:nowrap;'><b>$Old_Value</b> (<b>$Old_Size</b> bytes)</span> to <span style='white-space:nowrap;'><b>$New_Value</b> (<b>$New_Size</b> bytes)</span>.\n";
                                $Effect = "Layout of parameter's stack has been changed and therefore parameters in higher positions in the stack may be incorrectly initialized by applications.";
                            }
                        }
                        elsif($Kind eq "Parameter_BaseType")
                        {
                            if($InitialType_Type eq "Pointer")
                            {
                                $Incompatibility = "Base type of $Parameter_Position_Str parameter <b>$Target</b> (pointer) has been changed from <span style='white-space:nowrap;'><b>$Old_Value</b></span> to <span style='white-space:nowrap;'><b>$New_Value</b></span>.\n";
                                $Effect = "Memory stored by pointer may be incorrectly initialized by applications.";
                            }
                            else
                            {
                                $Incompatibility = "Base type of $Parameter_Position_Str parameter <b>$Target</b> has been changed from <span style='white-space:nowrap;'><b>$Old_Value</b></span> to <span style='white-space:nowrap;'><b>$New_Value</b></span>.\n";
                                $Effect = "Replacement of parameter base type may indicate a change in the semantic meaning of this parameter.";
                            }
                        }
                        elsif($Kind eq "Parameter_PointerLevel")
                        {
                            $Incompatibility = "Type pointer level of $Parameter_Position_Str parameter <b>$Target</b> has been changed from <b>$Old_Value</b> to <b>$New_Value</b>.\n";
                            $Effect = "Possible incorrect initialization of $Parameter_Position_Str parameter <b>$Target</b> by applications.";
                        }
                        elsif($Kind eq "Return_Type_And_Size")
                        {
                            $Incompatibility = "Type of return value has been changed from <span style='white-space:nowrap;'><b>$Old_Value</b> (<b>$Old_Size</b> bytes)</span> to <span style='white-space:nowrap;'><b>$New_Value</b> (<b>$New_Size</b> bytes)</span>.\n";
                            $Effect = "Applications will get a different return value and execution may change.";
                        }
                        elsif($Kind eq "Return_Type")
                        {
                            $Incompatibility = "Type of return value has been changed from <span style='white-space:nowrap;'><b>$Old_Value</b></span> to <span style='white-space:nowrap;'><b>$New_Value</b></span>.\n";
                            $Effect = "Replacement of return type may indicate a change in its semantic meaning.";
                        }
                        elsif($Kind eq "Return_BaseType_And_Size")
                        {
                            $Incompatibility = "Base type of return value has been changed from <span style='white-space:nowrap;'><b>$Old_Value</b> (<b>$Old_Size</b> bytes)</span> to <span style='white-space:nowrap;'><b>$New_Value</b> (<b>$New_Size</b> bytes)</span>.\n";
                            $Effect = "Applications will get a different return value and execution may change.";
                        }
                        elsif($Kind eq "Return_BaseType")
                        {
                            $Incompatibility = "Base type of return value has been changed from <span style='white-space:nowrap;'><b>$Old_Value</b></span> to <span style='white-space:nowrap;'><b>$New_Value</b></span>.\n";
                            $Effect = "Replacement of return base type may indicate a change in its semantic meaning.";
                        }
                        elsif($Kind eq "Return_PointerLevel")
                        {
                            $Incompatibility = "Type pointer level of return value has been changed from <b>$Old_Value</b> to <b>$New_Value</b>.\n";
                            $Effect = "Applications will get a different return value and execution may change.";
                        }
                        if($Incompatibility)
                        {
                            $InterfaceProblemsReport .= "<tr><td align='center' class='table_header'><span class='problem_num'>$ProblemNum</span></td><td align='left' valign='top'><span class='problem_body'>".$Incompatibility."</span></td><td align='left' valign='top'><span class='problem_body'>".$Effect."</span></td></tr>\n";
                            $ProblemNum += 1;
                            $Problems_Number += 1;
                        }
                    }
                }
                $ProblemNum -= 1;
                if($InterfaceProblemsReport)
                {
                    if($Interface =~ /\A_Z/)
                    {
                        if($Signature)
                        {
                            $HEADER_LIB_REPORT .= $ContentSpanStart."<span class='extension'>[+]</span> ".highLight_Signature_Italic_Color(htmlSpecChars($Signature))." ($ProblemNum)".$ContentSpanEnd."<br/>\n$ContentDivStart<span class='mangled'>[ symbol: <b>$Interface</b> ]</span><br/>\n";
                        }
                        else
                        {
                            $HEADER_LIB_REPORT .= $ContentSpanStart."<span class='extension'>[+]</span> ".$Interface." ($ProblemNum)".$ContentSpanEnd."<br/>\n$ContentDivStart\n";
                        }
                    }
                    else
                    {
                        if($Signature)
                        {
                            $HEADER_LIB_REPORT .= $ContentSpanStart."<span class='extension'>[+]</span> ".highLight_Signature_Italic_Color(htmlSpecChars($Signature))." ($ProblemNum)".$ContentSpanEnd."<br/>\n$ContentDivStart\n";
                        }
                        else
                        {
                            $HEADER_LIB_REPORT .= $ContentSpanStart."<span class='extension'>[+]</span> ".$Interface." ($ProblemNum)".$ContentSpanEnd."<br/>\n$ContentDivStart\n";
                        }
                    }
                    $HEADER_LIB_REPORT .= "<table width='900px' cellpadding='3' cellspacing='0' class='problems_table'><tr><td align='center' width='2%' class='table_header'><span class='problem_title' style='white-space:nowrap;'></span></td><td width='47%' align='center' class='table_header'><span class='problem_sub_title'>Incompatibility</span></td><td align='center' class='table_header'><span class='problem_sub_title'>Effect</span></td></tr>$InterfaceProblemsReport</table><br/>$ContentDivEnd\n";
                    $HEADER_LIB_REPORT = insertIDs($HEADER_LIB_REPORT);
                }
            }
            if($HEADER_LIB_REPORT)
            {
                $INTERFACE_PROBLEMS .= "<span class='header_name'>$HeaderName</span>, <span class='solib_name'>$SoName</span><br/>\n".$HEADER_LIB_REPORT."<br/>";
            }
        }
    }
    if($INTERFACE_PROBLEMS)
    {
        $INTERFACE_PROBLEMS = "<a name=\'Interface_Problems_$TargetPriority\'></a>\n<h2 style='margin-bottom:0px;padding-bottom:0px;'>Interface problems, $TargetPriority risk ($Problems_Number)</h2><hr/>\n"."<!--Interface_Problems_".$TargetPriority."-->\n".$INTERFACE_PROBLEMS."<!--Interface_Problems_".$TargetPriority."_End-->\n"."<input id=\'Interface_Problems_$TargetPriority"."_Count"."\' type='hidden' value=\'$Problems_Number\' /><a style='font-size:11px;' href='#Top'>to the top</a><br/>\n";
    }
    return $INTERFACE_PROBLEMS;
}

sub get_Report_TypeProblems($)
{
    my $TargetPriority = $_[0];
    my ($TYPE_PROBLEMS, %TypeHeader, %TypeChanges, %Type_MaxPriority) = ();
    foreach my $Interface (sort keys(%CompatProblems))
    {
        foreach my $Kind (keys(%{$CompatProblems{$Interface}}))
        {
            if($TypeProblems_Kind{$Kind})
            {
                foreach my $Location (keys(%{$CompatProblems{$Interface}{$Kind}}))
                {
                    my $Type_Name = $CompatProblems{$Interface}{$Kind}{$Location}{"Type_Name"};
                    my $Priority = $CompatProblems{$Interface}{$Kind}{$Location}{"Priority"};
                    my $Type_Header = $CompatProblems{$Interface}{$Kind}{$Location}{"Header"};
                    %{$TypeChanges{$Type_Name}{$Kind}{$Location}} = %{$CompatProblems{$Interface}{$Kind}{$Location}};
                    $TypeHeader{$Type_Header}{$Type_Name} = 1;
                    $Type_MaxPriority{$Type_Name}{$Kind} = max_priority($Type_MaxPriority{$Type_Name}{$Kind}, $Priority);
                }
            }
        }
    }
    foreach my $Type_Name (keys(%TypeChanges))
    {
        foreach my $Kind (keys(%{$TypeChanges{$Type_Name}}))
        {
            foreach my $Location (keys(%{$TypeChanges{$Type_Name}{$Kind}}))
            {
                my $Priority = $TypeChanges{$Type_Name}{$Kind}{$Location}{"Priority"};
                if(cmp_priority($Type_MaxPriority{$Type_Name}{$Kind}, $Priority))
                {
                    delete($TypeChanges{$Type_Name}{$Kind}{$Location});
                }
            }
        }
    }
    my $Problems_Number = 0;
    foreach my $HeaderName (sort {lc($a) cmp lc($b)} keys(%TypeHeader))
    {
        my $HEADER_REPORT = "";
        foreach my $TypeName (sort {lc($a) cmp lc($b)} keys(%{$TypeHeader{$HeaderName}}))
        {
            my $ProblemNum = 1;
            my $TypeProblemsReport = "";
            my %Kinds_Locations = ();
            my %Kinds_Target = ();
            foreach my $Kind (keys(%{$TypeChanges{$TypeName}}))
            {
                foreach my $Location (keys(%{$TypeChanges{$TypeName}{$Kind}}))
                {
                    my $Priority = $TypeChanges{$TypeName}{$Kind}{$Location}{"Priority"};
                    next if($Priority ne $TargetPriority);
                    $Kinds_Locations{$Kind}{$Location} = 1;
                    my $Incompatibility = "";
                    my $Effect = "";
                    my $Target = $TypeChanges{$TypeName}{$Kind}{$Location}{"Target"};
                    next if($Kinds_Target{$Kind}{$Target});
                    $Kinds_Target{$Kind}{$Target} = 1;
                    my $Old_Value = htmlSpecChars($TypeChanges{$TypeName}{$Kind}{$Location}{"Old_Value"});
                    my $New_Value = htmlSpecChars($TypeChanges{$TypeName}{$Kind}{$Location}{"New_Value"});
                    my $Old_Size = $TypeChanges{$TypeName}{$Kind}{$Location}{"Old_Size"};
                    my $New_Size = $TypeChanges{$TypeName}{$Kind}{$Location}{"New_Size"};
                    my $Type_Type = $TypeChanges{$TypeName}{$Kind}{$Location}{"Type_Type"};
                    my $InitialType_Type = $TypeChanges{$TypeName}{$Kind}{$Location}{"InitialType_Type"};
                    
                    if($Kind eq "Added_Virtual_Function")
                    {
                        $Incompatibility = "Virtual method "."<span class='interface_name_black'>".highLight_Signature(htmlSpecChars($Target))."</span>"." has been added to this class and therefore the layout of virtual table has been changed.";
                        $Effect = "Call of any virtual method in this class or its subclasses will result in crash of application.";
                    }
                    elsif($Kind eq "Withdrawn_Virtual_Function")
                    {
                        $Incompatibility = "Virtual method "."<span class='interface_name_black'>".highLight_Signature(htmlSpecChars($Target))."</span>"." has been withdrawn from this class and therefore the layout of virtual table has been changed.";
                        $Effect = "Call of any virtual method in this class or its subclasses will result in crash of application.";
                    }
                    elsif($Kind eq "Virtual_Function_Position")
                    {
                        $Incompatibility = "The relative position of virtual method "."<span class='interface_name_black'>".highLight_Signature(htmlSpecChars($Target))."</span>"." has been changed from <b>$Old_Value</b> to <b>$New_Value</b> and therefore the layout of virtual table has been changed.";
                        $Effect = "Call of this virtual method will result in crash of application.";
                    }
                    elsif($Kind eq "Virtual_Function_Redefinition")
                    {
                        $Incompatibility = "Virtual method "."<span class='interface_name_black'>".highLight_Signature(htmlSpecChars($Old_Value))."</span>"." has been redefined by "."<span class='interface_name_black'>".highLight_Signature(htmlSpecChars($New_Value))."</span>";
                        $Effect = "Method <span class='interface_name_black'>".highLight_Signature(htmlSpecChars($New_Value))."</span> will be called instead of <span class='interface_name_black'>".highLight_Signature(htmlSpecChars($Old_Value))."</span>";
                    }
                    elsif($Kind eq "Virtual_Function_Redefinition_B")
                    {
                        $Incompatibility = "Virtual method "."<span class='interface_name_black'>".highLight_Signature(htmlSpecChars($New_Value))."</span>"." has been redefined by "."<span class='interface_name_black'>".highLight_Signature(htmlSpecChars($Old_Value))."</span>";
                        $Effect = "Method <span class='interface_name_black'>".highLight_Signature(htmlSpecChars($Old_Value))."</span> will be called instead of <span class='interface_name_black'>".highLight_Signature(htmlSpecChars($New_Value))."</span>";
                    }
                    elsif($Kind eq "Size")
                    {
                        $Incompatibility = "Size of this type has been changed from <b>$Old_Value</b> to <b>$New_Value</b> bytes.";
                        $Effect = "Change of type size may lead to different effects in different contexts. $ContentSpanStart"."<span style='color:Black'>[+] ...</span>"."$ContentSpanEnd <label id=\"CONTENT_ID\" style=\"display:none;\"> In the context of function parameters, this change affects the parameter's stack layout and may lead to incorrect initialization of parameters in higher positions in the stack. In the context of structure members, this change affects the member's layout and may lead to incorrect attempts to access members in higher positions. Other effects are possible.</label>";
                    }
                    elsif($Kind eq "BaseType")
                    {
                        $Incompatibility = "Base of this type has been changed from <b>$Old_Value</b> to <b>$New_Value</b>.";
                        $Effect = "Possible incorrect initialization of interface parameters by applications.";
                    }
                    elsif($Kind eq "Added_Member_And_Size")
                    {
                        $Incompatibility = "Member <b>$Target</b> has been added to this type.";
                        $Effect = "The size of the inclusive type has been changed.";
                    }
                    elsif($Kind eq "Added_Middle_Member_And_Size")
                    {
                        $Incompatibility = "Member <b>$Target</b> has been added between the first member and the last member of this structural type.";
                        $Effect = "1) Layout of structure members has been changed and therefore members in higher positions in the structure definition may be incorrectly accessed by applications.<br/>2) The size of the inclusive type will also be affected.";
                    }
                    elsif($Kind eq "Member_Rename")
                    {
                        $Incompatibility = "Member <b>$Target</b> has been renamed to <b>$New_Value</b>.";
                        $Effect = "Renaming of a member in a structural data type may indicate a change in the semantic meaning of the member.";
                    }
                    elsif($Kind eq "Withdrawn_Member_And_Size")
                    {
                        $Incompatibility = "Member <b>$Target</b> has been withdrawn from this type.";
                        $Effect = "1) Applications will access incorrect memory when attempting to access this member.<br/>2) The size of the inclusive type will also be affected.";
                    }
                    elsif($Kind eq "Withdrawn_Member")
                    {
                        $Incompatibility = "Member <b>$Target</b> has been withdrawn from this type.";
                        $Effect = "Applications will access incorrect memory when attempting to access this member.";
                    }
                    elsif($Kind eq "Withdrawn_Middle_Member_And_Size")
                    {
                        $Incompatibility = "Member <b>$Target</b> has been withdrawn from this structural type between the first member and the last member.";
                        $Effect = "1) Layout of structure members has been changed and therefore members in higher positions in the structure definition may be incorrectly accessed by applications.<br/>2) Previous accesses of applications to the withdrawn member will be incorrect.";
                    }
                    elsif($Kind eq "Withdrawn_Middle_Member")
                    {
                        $Incompatibility = "Member <b>$Target</b> has been withdrawn from this structural type between the first member and the last member.";
                        $Effect = "1) Layout of structure members has been changed and therefore members in higher positions in the structure definition may be incorrectly accessed by applications.<br/>2) Applications will access incorrect memory when attempting to access this member.";
                    }
                    elsif($Kind eq "Enum_Member_Value")
                    {
                        $Incompatibility = "Value of member <b>$Target</b> has been changed from <b>$Old_Value</b> to <b>$New_Value</b>.";
                        $Effect = "Applications may execute another branch of library code.";
                    }
                    elsif($Kind eq "Enum_Member_Name")
                    {
                        $Incompatibility = "Name of member with value <b>$Target</b> has been changed from <b>$Old_Value</b> to <b>$New_Value</b>.";
                        $Effect = "Applications may execute another branch of library code.";
                    }
                    elsif($Kind eq "Member_Type_And_Size")
                    {
                        $Incompatibility = "Type of member <b>$Target</b> has been changed from <span style='white-space:nowrap;'><b>$Old_Value</b> (<b>$Old_Size</b> bytes)</span> to <span style='white-space:nowrap;'><b>$New_Value</b> (<b>$New_Size</b> bytes)</span>.";
                        $Effect = "Layout of structure members has been changed and therefore members in higher positions in the structure definition may be incorrectly accessed by applications.";
                    }
                    elsif($Kind eq "Member_Type")
                    {
                        $Incompatibility = "Type of member <b>$Target</b> has been changed from <span style='white-space:nowrap;'><b>$Old_Value</b></span> to <span style='white-space:nowrap;'><b>$New_Value</b></span>.";
                        $Effect = "Replacement of the member data type may indicate a change in the semantic meaning of the member.";
                    }
                    elsif($Kind eq "Member_BaseType_And_Size")
                    {
                        if($InitialType_Type eq "Pointer")
                        {
                            $Incompatibility = "Base type of member <b>$Target</b> (pointer) has been changed from <span style='white-space:nowrap;'><b>$Old_Value</b> (<b>$Old_Size</b> bytes)</span> to <span style='white-space:nowrap;'><b>$New_Value</b> (<b>$New_Size</b> bytes)</span>.";
                            $Effect = "Possible access of applications to incorrect memory via member pointer.";
                        }
                        else
                        {
                            $Incompatibility = "Base type of member <b>$Target</b> has been changed from <span style='white-space:nowrap;'><b>$Old_Value</b> (<b>$Old_Size</b> bytes)</span> to <span style='white-space:nowrap;'><b>$New_Value</b> (<b>$New_Size</b> bytes)</span>.";
                            $Effect = "Layout of structure members has been changed and therefore members in higher positions in structure definition may be incorrectly accessed by applications.";
                        }
                    }
                    elsif($Kind eq "Member_BaseType")
                    {
                        if($InitialType_Type eq "Pointer")
                        {
                            $Incompatibility = "Base type of member <b>$Target</b> (pointer) has been changed from <span style='white-space:nowrap;'><b>$Old_Value</b></span> to <span style='white-space:nowrap;'><b>$New_Value</b></span>.";
                            $Effect = "Possible access of applications to incorrect memory via member pointer.";
                        }
                        else
                        {
                            $Incompatibility = "Base type of member <b>$Target</b> has been changed from <span style='white-space:nowrap;'><b>$Old_Value</b></span> to <span style='white-space:nowrap;'><b>$New_Value</b></span>.";
                            $Effect = "Replacement of member base type may indicate a change in the semantic meaning of this member.";
                        }
                    }
                    elsif($Kind eq "Member_PointerLevel")
                    {
                        $Incompatibility = "Type pointer level of member <b>$Target</b> has been changed from <b>$Old_Value</b> to <b>$New_Value</b>.";
                        $Effect = "Possible incorrect initialization of member <b>$Target</b> by applications.";
                    }
                    if($Incompatibility)
                    {
                        $TypeProblemsReport .= "<tr><td align='center' valign='top' class='table_header'><span class='problem_num'>$ProblemNum</span></td><td align='left' valign='top'><span class='problem_body'>".$Incompatibility."</span></td><td align='left' valign='top'><span class='problem_body'>$Effect</span></td></tr>\n";
                        $ProblemNum += 1;
                        $Problems_Number += 1;
                        $Kinds_Locations{$Kind}{$Location} = 1;
                    }
                }
            }
            $ProblemNum -= 1;
            if($TypeProblemsReport)
            {
                my ($Affected_Interfaces_Header, $Affected_Interfaces) = getAffectedInterfaces($TypeName, \%Kinds_Locations);
                $HEADER_REPORT .= $ContentSpanStart."<span class='extension'>[+]</span> ".htmlSpecChars($TypeName)." ($ProblemNum)".$ContentSpanEnd."<br/>\n$ContentDivStart<table width='900px' cellpadding='3' cellspacing='0' class='problems_table'><tr><td align='center' width='2%' class='table_header'><span class='problem_title' style='white-space:nowrap;'></span></td><td width='47%' align='center' class='table_header'><span class='problem_sub_title'>Incompatibility</span></td><td align='center' class='table_header'><span class='problem_sub_title'>Effect</span></td></tr>$TypeProblemsReport</table>"."<span style='padding-left:10px'>$Affected_Interfaces_Header</span>$Affected_Interfaces<br/><br/>$ContentDivEnd\n";
                $HEADER_REPORT = insertIDs($HEADER_REPORT);
            }
        }
        if($HEADER_REPORT)
        {
            $TYPE_PROBLEMS .= "<span class='header_name'>$HeaderName</span><br/>\n".$HEADER_REPORT."<br/>";
        }
    }
    if($TYPE_PROBLEMS)
    {
        my $Notations = "";
        if($TYPE_PROBLEMS =~ /'RetVal|'Obj/)
        {
            my @Notations_Array = ();
            if($TYPE_PROBLEMS =~ /'RetVal/)
            {
                @Notations_Array = (@Notations_Array, "<span style='color:#444444;padding-left:5px;'><b>RetVal</b></span> - function's return value");
            }
            if($TYPE_PROBLEMS =~ /'Obj/)
            {
                @Notations_Array = (@Notations_Array, "<span style='color:#444444;'><b>Obj</b></span> - method's object (C++)");
            }
            $Notations = "Shorthand notations: ".join("; ", @Notations_Array).".<br/>\n";
        }
        $TYPE_PROBLEMS = "<a name=\'Type_Problems_$TargetPriority\'></a>\n<h2 style='margin-bottom:0px;padding-bottom:0px;'>Problems in Data Types, $TargetPriority risk ($Problems_Number)</h2><hr/>\n".$Notations."<!--Type_Problems_".$TargetPriority."-->\n".$TYPE_PROBLEMS."<!--Type_Problems_".$TargetPriority."_End-->\n"."<input id=\'Type_Problems_$TargetPriority"."_Count"."\' type='hidden' value=\'$Problems_Number\' /><a style='font-size:11px;' href='#Top'>to the top</a><br/>\n";
    }
    return $TYPE_PROBLEMS;
}

my $ContentSpanStart_2 = "<span style='line-height:25px;' class=\"section_2\" onclick=\"javascript:showContent(this, 'CONTENT_ID')\">\n";

sub getAffectedInterfaces($$)
{
    my ($Target_TypeName, $Kinds_Locations) = @_;
    my ($Affected_Interfaces_Header, $Affected_Interfaces, %FunctionNumber) = ();
    foreach my $Interface (sort {lc($tr_name{$a}) cmp lc($tr_name{$b})} keys(%CompatProblems))
    {
        next if(($Interface =~ /C2/) or ($Interface =~ /D2/) or ($Interface =~ /D0/));
        next if(keys(%FunctionNumber)>1000);
        my $FunctionProblem = "";
        my $MinPath_Length = "";
        my $MaxPriority = 0;
        my $Location_Last = "";
        foreach my $Kind (keys(%{$CompatProblems{$Interface}}))
        {
            foreach my $Location (keys(%{$CompatProblems{$Interface}{$Kind}}))
            {
                next if(not $Kinds_Locations->{$Kind}{$Location});
                my $Type_Name = $CompatProblems{$Interface}{$Kind}{$Location}{"Type_Name"};
                my $Signature = $CompatProblems{$Interface}{$Kind}{$Location}{"Signature"};
                my $Parameter_Position = $CompatProblems{$Interface}{$Kind}{$Location}{"Parameter_Position"};
                my $Priority = $CompatProblems{$Interface}{$Kind}{$Location}{"Priority"};
                if($Type_Name eq $Target_TypeName)
                {
                    $FunctionNumber{$Interface} = 1;
                    my $Path_Length = 0;
                    while($Location =~ /\-\>/g){$Path_Length += 1;}
                    if(($MinPath_Length eq "") or ($Path_Length<$MinPath_Length and $Priority_Value{$Priority}>$MaxPriority) or (($Location_Last =~ /RetVal/ or $Location_Last =~ /Obj/) and $Location !~ /RetVal|Obj/ and $Location !~ /\-\>/) or ($Location_Last =~ /RetVal|Obj/ and $Location_Last =~ /\-\>/ and $Location !~ /RetVal|Obj/ and $Location =~ /\-\>/))
                    {
                        $MinPath_Length = $Path_Length;
                        $MaxPriority = $Priority_Value{$Priority};
                        $Location_Last = $Location;
                        my $Description = get_AffectDescription($Interface, $Kind, $Location);
                        $FunctionProblem = "<span class='interface_name_black' style='padding-left:20px;'>".highLight_Signature_PPos_Italic(htmlSpecChars($Signature), $Parameter_Position, 1, 0)."</span>:<br/>"."<span class='affect_description'>".addArrows($Description)."</span><br/><div style='height:4px;'>&nbsp;</div>\n";
                    }
                }
            }
        }
        $Affected_Interfaces .= $FunctionProblem;
    }
    $Affected_Interfaces .= "and other...<br/>" if(keys(%FunctionNumber)>5000);
    if($Affected_Interfaces)
    {
        $Affected_Interfaces_Header = $ContentSpanStart_2."[+] affected interfaces (".keys(%FunctionNumber).")".$ContentSpanEnd;
        $Affected_Interfaces =  $ContentDivStart.$Affected_Interfaces.$ContentDivEnd;
    }
    return ($Affected_Interfaces_Header, $Affected_Interfaces);
}

my %Kind_TypeStructureChanged=(
    "Size"=>1,
    "Added_Member_And_Size"=>1,
    "Added_Middle_Member_And_Size"=>1,
    "Member_Rename"=>1,
    "Withdrawn_Member_And_Size"=>1,
    "Withdrawn_Member"=>1,
    "Withdrawn_Middle_Member_And_Size"=>1,
    "Enum_Member_Value"=>1,
    "Enum_Member_Name"=>1,
    "Member_Type_And_Size"=>1,
    "Member_Type"=>1,
    "Member_BaseType_And_Size"=>1,
    "Member_BaseType"=>1,
    "Member_PointerLevel"=>1,
    "BaseType"=>1
);

my %Kind_VirtualTableChanged=(
    "Added_Virtual_Function"=>1,
    "Withdrawn_Virtual_Function"=>1,
    "Virtual_Function_Position"=>1,
    "Virtual_Function_Redefinition"=>1,
    "Virtual_Function_Redefinition_B"=>1
);

sub get_AffectDescription($$$)
{
    my ($Interface, $Kind, $Location) = @_;
    my $Target = $CompatProblems{$Interface}{$Kind}{$Location}{"Target"};
    my $Old_Value = $CompatProblems{$Interface}{$Kind}{$Location}{"Old_Value"};
    my $New_Value = $CompatProblems{$Interface}{$Kind}{$Location}{"New_Value"};
    my $Type_Name = $CompatProblems{$Interface}{$Kind}{$Location}{"Type_Name"};
    my $Parameter_Position = $CompatProblems{$Interface}{$Kind}{$Location}{"Parameter_Position"};
    my $Parameter_Name = $CompatProblems{$Interface}{$Kind}{$Location}{"Parameter_Name"};
    my $Parameter_Type_Name = $CompatProblems{$Interface}{$Kind}{$Location}{"Parameter_Type_Name"};
    my $Member_Type_Name = $CompatProblems{$Interface}{$Kind}{$Location}{"Member_Type_Name"};
    my $Object_Type_Name = $CompatProblems{$Interface}{$Kind}{$Location}{"Object_Type_Name"};
    my $Return_Type_Name = $CompatProblems{$Interface}{$Kind}{$Location}{"Return_Type_Name"};
    my $Start_Type_Name = $CompatProblems{$Interface}{$Kind}{$Location}{"Start_Type_Name"};
    my $InitialType_Type = $CompatProblems{$Interface}{$Kind}{$Location}{"InitialType_Type"};
    my $Parameter_Position_Str = num_to_str($Parameter_Position + 1);
    my @Sentence_Parts = ();
    my $Location_To_Type = $Location;
    $Location_To_Type =~ s/\-\>[^>]+?\Z//;
    if($Kind_VirtualTableChanged{$Kind})
    {
        if($Kind eq "Virtual_Function_Redefinition")
        {
            @Sentence_Parts = (@Sentence_Parts, "This method become virtual and will be called instead of redefined method '".highLight_Signature(htmlSpecChars($Old_Value))."'.");
        }
        elsif($Kind eq "Virtual_Function_Redefinition_B")
        {
            @Sentence_Parts = (@Sentence_Parts, "This method become non-virtual and redefined method '".highLight_Signature(htmlSpecChars($Old_Value))."' will be called instead of it.");
        }
        else
        {
            @Sentence_Parts = (@Sentence_Parts, "Call of this virtual method will result in crash of application because the layout of virtual table has been changed.");
        }
    }
    elsif($Kind_TypeStructureChanged{$Kind})
    {
        if($Location_To_Type =~ /RetVal/)
        {#return value
            if($Location_To_Type =~ /\-\>/)
            {
                @Sentence_Parts = (@Sentence_Parts, "Member \'$Location_To_Type\' in return value");
            }
            else
            {
                @Sentence_Parts = (@Sentence_Parts, "Return value");
            }
        }
        elsif($Location_To_Type =~ /Obj/)
        {#object
            if($Location_To_Type =~ /\-\>/)
            {
                @Sentence_Parts = (@Sentence_Parts, "Member \'$Location_To_Type\' in the object of this method");
            }
            else
            {
                @Sentence_Parts = (@Sentence_Parts, "Object");
            }
        }
        else
        {#parameters
            if($Location_To_Type =~ /\-\>/)
            {
                @Sentence_Parts = (@Sentence_Parts, "Member \'$Location_To_Type\' of $Parameter_Position_Str parameter");
            }
            else
            {
                @Sentence_Parts = (@Sentence_Parts, "$Parameter_Position_Str parameter");
            }
            if($Parameter_Name)
            {
                @Sentence_Parts = (@Sentence_Parts, "\'$Parameter_Name\'");
            }
            if($InitialType_Type eq "Pointer")
            {
                @Sentence_Parts = (@Sentence_Parts, "(pointer)");
            }
        }
        if($Start_Type_Name eq $Type_Name)
        {
            @Sentence_Parts = (@Sentence_Parts, "has type \'$Type_Name\'.");
        }
        else
        {
            @Sentence_Parts = (@Sentence_Parts, "has base type \'$Type_Name\'.");
        }
    }
    return join(" ", @Sentence_Parts);
}

sub create_HtmlReport()
{
    my $CssStyles = "<style type=\"text/css\">
    body{font-family:Arial;}
    hr{color:Black;background-color:Black;height:1px;border:0;}
    span.section{font-weight:bold;cursor:pointer;margin-left:7px;font-size:16px;font-family:Arial;color:#003E69;}
    span:hover.section{color:#336699;}
    span.section_2{cursor:pointer;margin-left:7px;font-size:14px;font-family:Arial;color:#cc3300;}
    span.extension{font-weight:100;font-size:16px;}
    span.problem_exact_location{color:Red;font-size:14px;}
    span.header_name{color:#cc3300;font-size:14px;font-family:Arial;font-weight:bold;}
    span.header_list_elem{padding-left:10px;color:#333333;font-size:15px;font-family:Arial;}
    span.solib_list_elem{padding-left:10px;color:#333333;font-size:15px;font-family:Arial;}
    span.solib_name{color:Green;font-size:14px;font-family:Arial;font-weight:bold;}
    span.interface_name{font-weight:bold;font-size:16px;font-family:Arial;color:#003E69;margin-left:7px;}
    span.interface_name_black{font-weight:bold;font-size:15px;font-family:Arial;color:#333333;}
    span.problem_title{color:#333333;font-weight:bold;font-size:13px;font-family:Verdana;}
    span.problem_sub_title{color:#333333;text-decoration:none;font-weight:bold;font-size:13px;font-family:Verdana;}
    span.problem_body{color:Black;font-size:14px;}
    span.int_p{font-weight:normal;}
    span.affect_description{padding-left:30px;font-size:14px;font-style:italic;line-height:13px;}
    table.problems_table{line-height:16px;margin-left:15px;margin-top:3px;border-collapse:collapse;}
    table.problems_table td{border-style:solid;border-color:Gray;border-width:1px;}
    td.table_header{background-color:#eeeeee;}
    td.summary_item{font-size:15px;font-family:Arial;text-align:left;}
    td.summary_item_value{padding-left:5px;padding-right:5px;width:35px;text-align:right;}
    span.problem_num{color:#333333;font-weight:bold;font-size:13px;font-family:Verdana;}
    span.mangled{padding-left:15px;font-size:13px;cursor:text;color:#444444;}
    span.symver{color:#555555;font-size:13px;white-space:nowrap;}
    span.color_param{font-style:italic;color:Brown;}
    span.focus_param{font-style:italic;color:Red;}</style>";
    
    my $JScripts = "<script type=\"text/javascript\" language=\"JavaScript\">
    function showContent(header, id)   {
        e = document.getElementById(id);
        if(e.style.display == 'none')
        {
            e.style.display = '';
            e.style.visibility = 'visible';
            header.innerHTML = header.innerHTML.replace(/\\\[[^0-9 ]\\\]/gi,\"[&minus;]\");
        }
        else
        {
            e.style.display = 'none';
            e.style.visibility = 'hidden';
            header.innerHTML = header.innerHTML.replace(/\\\[[^0-9 ]\\\]/gi,\"[+]\");
        }
    }</script>";
    
    writeFile("$REPORT_PATH/abi_compat_report.html", "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n<head>\n<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />
    <title>ABI compliance report for the library $TargetLibraryName from version ".$Descriptor{1}{"Version"}." to ".$Descriptor{2}{"Version"}." on ".getArch()."\n</title>\n<!--Styles-->\n".$CssStyles."\n<!--Styles_End-->\n"."<!--JScripts-->\n".$JScripts."\n<!--JScripts_End-->\n</head>\n<body>\n<div><a name='Top'></a>\n".get_Report_Header()."<br/>\n".get_Summary()."<br/>\n".get_Report_Added().get_Report_Withdrawn().get_Report_TypeProblems("High").get_Report_TypeProblems("Medium").get_Report_TypeProblems("Low").get_Report_InterfaceProblems("High").get_Report_InterfaceProblems("Medium").get_Report_InterfaceProblems("Low").get_Report_ChangedConstants().get_SourceInfo()."</div>\n"."<br/><br/><br/><hr/><div style='width:100%;font-family:Arial;font-size:11px;' align='right'><i>Generated on ".(localtime time)." for <span style='font-weight:bold'>$TargetLibraryName</span> by <a href='http://ispras.linux-foundation.org/index.php/ABI_compliance_checker'>ABI-compliance-checker</a> $ABI_COMPLIANCE_CHECKER_VERSION &nbsp;</i></div>\n<div style='height:999px;'></div>\n</body></html>");
}

sub trivialCmp($$)
{
    if(int($_[0]) > int($_[1]))
    {
        return 1;
    }
    elsif($_[0] eq $_[1])
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

sub addArrows($)
{
    my $Text = $_[0];
    #$Text =~ s/\-\>/&#8594;/g;
    $Text =~ s/\-\>/&minus;&gt;/g;
    return $Text;
}

sub insertIDs($)
{
    my $Text = $_[0];
    while($Text =~ /CONTENT_ID/)
    {
        if(int($Content_Counter)%2)
        {
            $ContentID -= 1;
        }
        $Text =~ s/CONTENT_ID/c_$ContentID/;
        $ContentID += 1;
        $Content_Counter += 1;
    }
    return $Text;
}

sub restrict_num_decimal_digits
{
  my $num=shift;
  my $digs_to_cut=shift;

  if ($num=~/\d+\.(\d){$digs_to_cut,}/)
  {
    $num=sprintf("%.".($digs_to_cut-1)."f", $num);
  }
  return $num;
}

sub parse_constants()
{
    my $CurHeader = "";
    foreach my $String (split("\n", $ConstantsSrc{$Version}))
    {#detecting public and private constants using sources
        if($String=~/#[ \t]+\d+[ \t]+\"(.+)\"/)
        {
            $CurHeader=$1;
        }
        if($String=~/\#[ \t]*define[ \t]+([_A-Z]+)[ \t]+(.*)[ \t]*\Z/)
        {
            my ($Name, $Value) = ($1, $2);
            if(not $Constants{$Version}{$Name}{"Access"})
            {
                $Constants{$Version}{$Name}{"Access"} = "public";
                $Constants{$Version}{$Name}{"Value"} = $Value;
                $Constants{$Version}{$Name}{"Header"} = get_FileName($CurHeader);
            }
        }
        elsif($String=~/\#[ \t]*undef[ \t]+([_A-Z]+)[ \t]*/)
        {
            my $Name = $1;
            $Constants{$Version}{$Name}{"Access"} = "private";
        }
    }
    foreach my $Constant (keys(%{$Constants{$Version}}))
    {
        if(($Constants{$Version}{$Constant}{"Access"} eq "private") or not $Constants{$Version}{$Constant}{"Value"}
        or $Constant=~/_h\Z/i)
        {
            delete($Constants{$Version}{$Constant});
        }
        else
        {
            delete($Constants{$Version}{$Constant}{"Access"});
        }
    }
}

sub mergeConstants()
{
    return if(defined $AppPath);
    foreach my $Constant (keys(%{$Constants{1}}))
    {
        my $Old_Value = $Constants{1}{$Constant}{"Value"};
        my $New_Value = $Constants{2}{$Constant}{"Value"};
        my $Header = $Constants{1}{$Constant}{"Header"};
        if($New_Value and $Old_Value and ($New_Value ne $Old_Value))
        {
            %{$ConstantProblems{$Constant}} = (
                "Old_Value"=>$Old_Value,
                "New_Value"=>$New_Value,
                "Header"=>$Header
            );
        }
    }
}

sub mergeHeaders_Separately()
{
    system("mkdir", "-p", "header_compile_errors/".$TargetLibraryName);
    system("rm", "-fr", $ERR_PATH{1}, $ERR_PATH{2});
    my ($Header_Num, $Prev_Header_Length) = (0, 0);
    my $All_Count = keys(%{$Headers{1}});
	foreach my $Header_Dest (sort {int($Headers{1}{$a}{"Position"})<=>int($Headers{1}{$b}{"Position"})} keys(%{$Headers{1}}))
	{
        my $Header_Name = $Headers{1}{$Header_Dest}{"Name"};
        my $Dest_Count = keys(%{$HeaderName_Destinations{1}{$Header_Name}});
        my $Identity = $Headers{1}{$Header_Dest}{"Identity"};
        my $Dest_Comment = ($Dest_Count>1 and $Identity=~/\//)?" ($Identity)":"";
        print get_one_step_title($Header_Name.$Dest_Comment, $Header_Num, $All_Count, $Prev_Header_Length, 1)."\r";
 		%TypeDescr = ();
 		%FuncDescr = ();
 		%ClassFunc = ();
        %ClassVirtFunc = ();
 		%LibInfo = ();
        %CompleteSignature = ();
        %Cache = ();
        $Version = 1;
		parseHeader($Header_Dest);
        $Version = 2;
        my $PairHeaderDest = find_pair_header($Header_Dest);
        if(not $PairHeaderDest)
        {
            $Header_Num += 1;
            next;
        }
        parseHeader($PairHeaderDest);
        mergeSignatures();
        mergeConstants();
        $Header_Num += 1;
        $Prev_Header_Length = length($Header_Name.$Dest_Comment);
	}
    print get_one_step_title("", $All_Count, $All_Count, $Prev_Header_Length, 0)."\n";
}

sub get_one_step_title($$$$$)
{
    my ($Header_Name, $Num, $All_Count, $SpacesAtTheEnd, $ShowCurrent) = @_;
    my ($Spaces_1, $Spaces_2, $Title) = ();
    my $Title_1 = "checking headers: $Num/$All_Count [".restrict_num_decimal_digits($Num*100/$All_Count, 3)."%]".(($ShowCurrent)?",":"");
    foreach (0 .. length("checking headers: ")+length($All_Count)*2+11 - length($Title_1))
    {
        $Spaces_1 .= " ";
    }
    $Title .= $Title_1.$Spaces_1;
    if($ShowCurrent)
    {
        my $Title_2 = "current: $Header_Name";
        foreach (0 .. $SpacesAtTheEnd - length($Header_Name)-1)
        {
            $Spaces_2 .= " ";
        }
        $Title .= $Title_2.$Spaces_2;
    }
    else
    {
        foreach (0 .. $SpacesAtTheEnd + length(" current: ") - 1)
        {
            $Title .= " ";
        }
    }
    return $Title."\r";
}

sub find_pair_header($)
{
    my $Header_Dest = $_[0];
    my $Header_Name = $Headers{1}{$Header_Dest}{"Name"};
    my $Identity = $Headers{1}{$Header_Dest}{"Identity"};
    my @Pair_Dest = keys(%{$HeaderName_Destinations{2}{$Header_Name}});
    if($#Pair_Dest==0)
    {
        return $Pair_Dest[0];
    }
    elsif($#Pair_Dest==-1)
    {
        return "";
    }
    else
    {
        foreach my $Pair_Dest (@Pair_Dest)
        {
            my $Pair_Identity = $Headers{2}{$Pair_Dest}{"Identity"};
            if($Identity eq $Pair_Identity)
            {
                return $Pair_Dest;
            }
        }
        return "";
    }
}

sub getSymbols($)
{
    my $LibVersion = $_[0];
    my @SoLibPaths = getSoPaths($LibVersion);
    if($#SoLibPaths eq -1 and not $CheckHeadersOnly)
    {
        print "ERROR: shared objects were not found\n";
        exit(1);
    }
    foreach my $SoLibPath (@SoLibPaths)
    {
        getSymbols_Lib($LibVersion, $SoLibPath, 0);
    }
}

sub separatePath($)
{
    return ("", $_[0])if($_[0] !~ /\//);
    if($_[0] =~ /\A(.*\/)([^\/]*)\Z/)
    {
        return ($1, $2);
    }
    else
    {
        return ("", $_[0]);
    }
}

sub translateSymbols($)
{
    my $LibVersion = $_[0];
    my (@MnglNames, @UnMnglNames) = ();
    foreach my $Interface (sort keys(%{$Interface_Library{$LibVersion}}))
    {
        if($Interface =~ /\A_Z/)
        {
            $Interface =~ s/[\@]+(.*)\Z//;
            push(@MnglNames, $Interface);
        }
        else
        {
            $tr_name{$Interface} = $Interface;
            $mangled_name{$tr_name{$Interface}} = $Interface;
        }
    }
    if($#MnglNames > -1)
    {
        @UnMnglNames = reverse(unmangleArray(@MnglNames));
        foreach my $Interface (sort keys(%{$Interface_Library{$LibVersion}}))
        {
            if($Interface =~ /\A_Z/)
            {
                $Interface =~ s/[\@]+(.*)\Z//;
                $tr_name{$Interface} = pop(@UnMnglNames);
                $mangled_name{correctName($tr_name{$Interface})} = $Interface;
            }
        }
    }
}

sub detectAdded()
{
    #detecting added
    foreach my $Interface (keys(%{$Interface_Library{2}}))
    {
        if(not $Interface_Library{1}{$Interface})
        {
            $AddedInt{$Interface} = 1;
            my ($MnglName, $SymbolVersion) = ($Interface, "");
            if($Interface =~ /\A(.+)[\@]+(.+)\Z/)
            {
                ($MnglName, $SymbolVersion) = ($1, $2);
            }
            $FuncAttr{2}{$Interface}{"Signature"} = $tr_name{$MnglName}.(($SymbolVersion)?"\@".$SymbolVersion:"");
        }
    }
}

sub detectWithdrawn()
{
    #detecting withdrawn
    foreach my $Interface (keys(%{$Interface_Library{1}}))
    {
        if(not $Interface_Library{2}{$Interface} and not $Interface_Library{2}{$SymVer{2}{$Interface}})
        {
            next if($DepInterfaces{2}{$Interface});
            $WithdrawnInt{$Interface} = 1;
            my ($MnglName, $SymbolVersion) = ($Interface, "");
            if($Interface =~ /\A(.+)[\@]+(.+)\Z/)
            {
                ($MnglName, $SymbolVersion) = ($1, $2);
            }
            $FuncAttr{1}{$Interface}{"Signature"} = $tr_name{$MnglName}.(($SymbolVersion)?"\@".$SymbolVersion:"");
        }
    }
}

sub isRecurLib($)
{
    my $LibName = $_[0];
    foreach my $LibNameInStack (@RecurLib)
    {
        if($LibName eq $LibNameInStack)
        {
            return 1;
        }
    }
    return 0;
}

sub getSymbols_App($)
{
    my $Path = $_[0];
    return () if(not $Path or not -f $Path);
    my @Ints = ();
    open(APP, "readelf -WhlSsdA $Path |");
    my $symtab=0;#indicates that we are processing 'symtab' section of 'readelf' output
    while(<APP>)
    {
        if($symtab == 1) {
            #do nothing with symtab (but there are some plans for the future)
            next;
        }
        if( /'.dynsym'/ ) {
            $symtab=0;
        }
        elsif( /'.symtab'/ ) {
            $symtab=1;
        }
        elsif(my ($fullname, $idx, $Ndx) = readlile_ELF($_)) {
            if( $Ndx eq "UND" ) {
                #only exported interfaces
                push(@Ints, $fullname);
            }
        }
    }
    close(APP);
    return @Ints;
}

sub readlile_ELF($)
{
    if($_[0]=~/\s*\d+:\s+(\w*)\s+\w+\s+(\w+)\s+(\w+)\s+(\w+)\s+(\w+)\s((\w|@|\.)+)/)
    {#the line of 'readelf' output corresponding to the interface
        my ($value, $type, $bind, $vis, $Ndx, $fullname)=($1, $2, $3, $4, $5, $6);
        if(($bind ne "WEAK") and ($bind ne "GLOBAL")) {
            return ();
        }
        if(($type ne "FUNC") and ($type ne "OBJECT") and ($type ne "COMMON")) {
            return ();
        }
        if($vis ne "DEFAULT") {
            return ();
        }
        if(($Ndx eq "ABS") and ($value !~ /\D|1|2|3|4|5|6|7|8|9/)) {
            return ();
        }
        return ($fullname, $value, $Ndx);
    }
    else
    {
        return ();
    }
}

sub getSymbols_Lib($$$)
{
    my ($LibVersion, $Lib_Path, $IsNeededLib) = @_;
    return if(not $Lib_Path or not -f $Lib_Path);
    my ($Lib_Dir, $Lib_SoName) = separatePath($Lib_Path);
    return if($CheckedSoLib{$LibVersion}{$Lib_SoName} and $IsNeededLib);
    return if(isRecurLib($Lib_SoName) or $#RecurLib>=1);
    $CheckedSoLib{$LibVersion}{$Lib_SoName} = 1;
    push(@RecurLib, $Lib_SoName);
    my (%Value_Interface, %Interface_Value, %NeededLib) = ();
    if(not $IsNeededLib)
    {
        $SoNames_All{$LibVersion}{$Lib_SoName} = 1;
    }
    open(SOLIB, "readelf -WhlSsdA $Lib_Path |");
    my $symtab=0;#indicates that we are processing 'symtab' section of 'readelf' output
    while(<SOLIB>)
    {
        if($symtab == 1) {
            #do nothing with symtab (but there are some plans for the future)
            next;
        }
        if(/'.dynsym'/) {
            $symtab=0;
        }
        elsif(/'.symtab'/) {
            $symtab=1;
        }
        elsif(/NEEDED.+\[([^\[\]]+)\]/)
        {
            $NeededLib{$1} = 1;
        }
        elsif(my ($fullname, $idx, $Ndx) = readlile_ELF($_)) {
            if( $Ndx eq "UND" ) {
                #ignore interfaces that are exported form somewhere else
                next;
            }
            my ($realname, $version) = ($fullname, "");
            if($fullname =~ /\A([^@]+)[\@]+([^@]+)\Z/)
            {
                ($realname, $version) = ($1, $2);
            }
            next if(defined $InterfacesListPath and not $InterfacesList{$realname});
            next if(defined $AppPath and not $InterfacesList_App{$realname});
            if($IsNeededLib)
            {
                $DepInterfaces{$LibVersion}{$fullname} = 1;
            }
            if(not $IsNeededLib or (defined $InterfacesListPath and $InterfacesList{$realname}) or (defined $AppPath and $InterfacesList_App{$realname}))
            {
                $Interface_Library{$LibVersion}{$fullname} = $Lib_SoName;
                $Library_Interface{$LibVersion}{$Lib_SoName}{$fullname} = 1;
                $Interface_Value{$LibVersion}{$fullname} = $idx;
                $Value_Interface{$LibVersion}{$idx}{$fullname} = 1;
                if(not $Language{$LibVersion}{$Lib_SoName})
                {
                    if($fullname =~ /\A_Z[A-Z]*[0-9]+/)
                    {
                        $Language{$LibVersion}{$Lib_SoName} = "C++";
                    }
                }
            }
        }
    }
    close(SOLIB);
    if(not $IsNeededLib)
    {
        foreach my $Interface_Name (keys(%{$Interface_Library{$LibVersion}}))
        {
            next if($Interface_Name !~ /\@/);
            my $Interface_SymName = "";
            foreach my $InterfaceName_SameValue (keys(%{$Value_Interface{$LibVersion}{$Interface_Value{$LibVersion}{$Interface_Name}}}))
            {
                if($InterfaceName_SameValue ne $Interface_Name)
                {
                    $SymVer{$LibVersion}{$InterfaceName_SameValue} = $Interface_Name;
                    $Interface_SymName = $InterfaceName_SameValue;
                    last;
                }
            }
            if(not $Interface_SymName)
            {
                if($Interface_Name =~ /\A([^@]*)[\@]+([^@]*)\Z/ and not $SymVer{$LibVersion}{$1})
                {
                    $SymVer{$LibVersion}{$1} = $Interface_Name;
                }
            }
        }
    }
    foreach my $SoLib (keys(%NeededLib))
    {
        getSymbols_Lib($LibVersion, find_solib_path($Lib_Dir, $SoLib), 1);
    }
    pop(@RecurLib);
}

sub find_solib_path($$)
{
    my ($Dir, $SoName) = @_;
    $Dir=~s/\/\Z//g;
    if(-f $Dir."/".$SoName)
    {
        return $Dir."/".$SoName;
    }
    else
    {
        return $SoLib_DefaultPath{$SoName};
    }
}

sub symbols_Preparation($)
{#recreate %SoNames and %Language using info from *.abi file
    my $LibVersion = $_[0];
    foreach my $Lib_SoName (keys(%{$Library_Interface{$LibVersion}}))
    {
        foreach my $Interface_Name (keys(%{$Library_Interface{$LibVersion}{$Lib_SoName}}))
        {
            $Interface_Library{$LibVersion}{$Interface_Name} = $Lib_SoName;
            $SoNames_All{$LibVersion}{$Lib_SoName} = 1;
            if(not $Language{$LibVersion}{$Lib_SoName})
            {
                if($Interface_Name =~ /\A_Z[A-Z]*[0-9]+/)
                {
                    $Language{$LibVersion}{$Lib_SoName} = "C++";
                }
            }
        }
    }
}

sub getSoPaths($)
{
    my $LibVersion = $_[0];
    my @SoPaths = ();
    foreach my $Dest (split("\n", $Descriptor{$LibVersion}{"Libs"}))
    {
        $Dest =~ s/\A\s+|\s+\Z//g;
        next if(not $Dest);
        if(not -e $Dest)
        {
            print "ERROR: can't access \'$Dest\'\n";
        }
        my @SoPaths_Dest = getSOPaths_Dest($Dest);
        foreach (@SoPaths_Dest)
        {
            push(@SoPaths, $_);
        }
    }
    return @SoPaths;
}

sub getSOPaths_Dest($)
{
    my $Dest = $_[0];
    if(-f $Dest)
    {
        return $Dest;
    }
    my @AllObjects = cmd_find($Dest,"f","*\.so*");
    my @SOPaths = ();
    foreach my $SharedObject (@AllObjects)
    {
        if(cmd_file($SharedObject) =~ /shared object/)
        {
            @SOPaths = (@SOPaths, $SharedObject);
        }
    }
    return @SOPaths;
}

sub genDescriptorTemplate()
{
    writeFile("library-descriptor.xml", $Descriptor_Template."\n");
    print "descriptor template named 'library-descriptor.xml' has been generated in the current directory\n";
}

sub detectPointerSize()
{
    `mkdir -p temp`;
    writeFile("temp/get_pointer_size.c", "#include <stdio.h>
int main()
{
    printf(\"\%d\", sizeof(int*));
    return 0;
}\n");
    system("gcc temp/get_pointer_size.c -o temp/get_pointer_size");
    $POINTER_SIZE = `./temp/get_pointer_size`;
    `rm -fr temp`;
}

sub data_Preparation($)
{
    my $LibVersion = $_[0];
    if($Descriptor{$LibVersion}{"Path"} =~ /\.abi\.tar\.gz/)
    {
        my $FileName = cmd_tar($Descriptor{$LibVersion}{"Path"});
        if($FileName =~ /\.abi/)
        {
            chomp($FileName);
            my $LibraryABI = eval readFile($FileName);
            system("rm", "-f", $FileName);
            $TypeDescr{$LibVersion} = $LibraryABI->{"TypeDescr"};
            $FuncDescr{$LibVersion} = $LibraryABI->{"FuncDescr"};
            $Library_Interface{$LibVersion} = $LibraryABI->{"Interfaces"};
            $SymVer{$LibVersion} = $LibraryABI->{"SymVer"};
            $Tid_TDid{$LibVersion} = $LibraryABI->{"Tid_TDid"};
            $Descriptor{$LibVersion}{"Version"} = $LibraryABI->{"LibraryVersion"};
            $OpaqueTypes{$LibVersion} = $LibraryABI->{"OpaqueTypes"};
            $InternalInterfaces{$LibVersion} = $LibraryABI->{"InternalInterfaces"};
            $Headers{$LibVersion} = $LibraryABI->{"Headers"};
            $SoNames_All{$LibVersion} = $LibraryABI->{"SharedObjects"};
            $Constants{$LibVersion} = $LibraryABI->{"Constants"};
            if($LibraryABI->{"ABI_COMPLIANCE_CHECKER_VERSION"} ne $ABI_COMPLIANCE_CHECKER_VERSION)
            {
                print "ERROR: incompatible version of specified ABI dump (allowed only $ABI_COMPLIANCE_CHECKER_VERSION)\n";
                exit(1);
            }
            foreach my $Destination (keys(%{$Headers{$LibVersion}}))
            {
                my $Header = get_FileName($Destination);
                $HeaderName_Destinations{$LibVersion}{$Header}{$Destination} = 1;
            }
            symbols_Preparation($LibVersion);
        }
    }
    elsif($Descriptor{$LibVersion}{"Path"} =~ /\.tar\.gz\Z/)
    {
        print "ERROR: descriptor must be an XML file or '*.abi.tar.gz' ABI dump\n";
        exit(1);
    }
    else
    {
        readDescriptor($LibVersion);
        if(not $CheckHeadersOnly)
        {
            getSymbols($LibVersion);
        }
        headerSearch($LibVersion);
    }
}

sub dump_sorting($)
{
    my $hash = $_[0];
    if((keys(%{$hash}))[0] =~ /\A[0-9]+\Z/)
    {
        return [sort {int($a) <=> int($b)} keys %{$hash}];
    }
    else
    {
        return [sort {$a cmp $b} keys %{$hash}];
    }
}

sub get_solib_default_paths()
{
    foreach my $Line (split("\n", `ldconfig -p`))
    {
        if($Line=~/\A[ \t]*([^ \t]+) .* \=\> (.+)\Z/)
        {
            $SoLib_DefaultPath{$1} = $2;
        }
    }
}

sub scenario()
{
    if(defined $Help)
    {
        HELP_MESSAGE();
        exit(0);
    }
    if(defined $ShowVersion)
    {
        print "ABI Compliance Checker $ABI_COMPLIANCE_CHECKER_VERSION\nCopyright (C) The Linux Foundation\nCopyright (C) Institute for System Programming, RAS\nLicense GPLv2: GNU GPL version 2 <http://www.gnu.org/licenses/>\nThis program is free software: you can redistribute it and/or modify it.\n\nWritten by Andrey Ponomarenko.\n";
        exit(0);
    }
    $Data::Dumper::Sortkeys = \&dump_sorting;
    if(defined $TestSystem)
    {
        testSystem_cpp();
        testSystem_c();
        exit(0);
    }
    if($GenerateDescriptor)
    {
        genDescriptorTemplate();
        exit(0);
    }
    if(not defined $TargetLibraryName)
    {
        print "select library name (option -l <name>)\n";
        exit(1);
    }
    if(defined $InterfacesListPath)
    {
        if(not -f $InterfacesListPath)
        {
            print "ERROR: can't access file $InterfacesListPath\n";
            exit(1);
        }
        foreach my $Interface (split("\n", readFile($InterfacesListPath)))
        {
            $InterfacesList{$Interface} = 1;
        }
    }
    if($AppPath)
    {
        if(-f $AppPath)
        {
            foreach my $Interface (getSymbols_App($AppPath))
            {
                $InterfacesList_App{$Interface} = 1;
            }
        }
        else
        {
            print "ERROR: can't access file \'$AppPath\'\n";
            exit(1);
        }
    }
    get_solib_default_paths();
    if($DumpInfo_DescriptorPath)
    {
        if(not -f $DumpInfo_DescriptorPath)
        {
            print "ERROR: can't access file \'$DumpInfo_DescriptorPath\'\n";
            exit(1);
        }
        $Descriptor{1}{"Path"} = $DumpInfo_DescriptorPath;
        readDescriptor(1);
        detectPointerSize();
        getSymbols(1);
        translateSymbols(1);
        headerSearch(1);
        parseHeaders_AllInOne(1);
        cleanData(1);
        my %LibraryABI = ();
        print "creating library ABI info dump ...\n";
        $LibraryABI{"TypeDescr"} = $TypeDescr{1};
        $LibraryABI{"FuncDescr"} = $FuncDescr{1};
        $LibraryABI{"Interfaces"} = $Library_Interface{1};
        $LibraryABI{"SymVer"} = $SymVer{1};
        $LibraryABI{"LibraryVersion"} = $Descriptor{1}{"Version"};
        $LibraryABI{"Library"} = $TargetLibraryName;
        $LibraryABI{"SharedObjects"} = $SoNames_All{1};
        $LibraryABI{"Tid_TDid"} = $Tid_TDid{1};
        $LibraryABI{"OpaqueTypes"} = $OpaqueTypes{1};
        $LibraryABI{"InternalInterfaces"} = $InternalInterfaces{1};
        $LibraryABI{"Headers"} = $Headers{1};
        $LibraryABI{"Constants"} = $Constants{1};
        $LibraryABI{"ABI_COMPLIANCE_CHECKER_VERSION"} = $ABI_COMPLIANCE_CHECKER_VERSION;
        my $InfoDump_FilePath = "abi_dumps/$TargetLibraryName";
        my $InfoDump_FileName = $TargetLibraryName."_".$Descriptor{1}{"Version"}.".abi";
        system("mkdir", "-p", $InfoDump_FilePath);
        system("rm", "-f", $InfoDump_FilePath."/".$InfoDump_FileName.".tar.gz");
        writeFile("$InfoDump_FilePath/$InfoDump_FileName", Dumper(\%LibraryABI));
        system("cd ".esc($InfoDump_FilePath)." && tar -cf ".esc($InfoDump_FileName).".tar ".esc($InfoDump_FileName));
        system("cd ".esc($InfoDump_FilePath)." && gzip ".esc($InfoDump_FileName).".tar --best");
        system("rm", "-f", $InfoDump_FilePath."/".$InfoDump_FileName);
        if($?)
        {
            print "ERROR: can't create library ABI info dump\n";
            exit(1);
        }
        else
        {
            print "see library ABI info dump in \'$InfoDump_FilePath/$InfoDump_FileName\.tar\.gz\': use it instead of library version descriptor on other machine\n";
            exit(0);
        }
    }
    if(not $Descriptor{1}{"Path"})
    {
        print "select 1st library descriptor (option -d1 <path>)\n";
        exit(1);
    }
    if(not -f $Descriptor{1}{"Path"})
    {
        print "ERROR: descriptor d1 does not exist, incorrect file path '".$Descriptor{1}{"Path"}."'\n";
        exit(1);
    }
    if(not $Descriptor{2}{"Path"})
    {
        print "select 2nd library descriptor (option -d2 <path>)\n";
        exit(1);
    }
    if(not -f $Descriptor{2}{"Path"})
    {
        print "ERROR: descriptor d2 does not exist, incorrect file path '".$Descriptor{2}{"Path"}."'\n";
        exit(1);
    }
    $StartTime = localtime time;
    print "preparation...\n";
    data_Preparation(1);
    data_Preparation(2);
    if($AppPath and not keys(%{$Interface_Library{1}}))
    {
        print "WARNING: symbols from the specified application were not found in the specified library shared objects\n";
    }
    $REPORT_PATH = "compat_reports/$TargetLibraryName/".$Descriptor{1}{"Version"}."_to_".$Descriptor{2}{"Version"};
    system("mkdir", "-p", $REPORT_PATH);
    system("rm", "-f", $REPORT_PATH."/abi_compat_report.html");
    detectPointerSize();
    translateSymbols(1);
    translateSymbols(2);
    if(not $CheckHeadersOnly)
    {
        detectAdded();
        detectWithdrawn();
    }
    #headers merging
    if($HeaderCheckingMode_Separately and $Descriptor{1}{"Path"} !~ /\.abi\.tar\.gz/ and $Descriptor{2}{"Path"} !~ /\.abi\.tar\.gz/)
    {
        mergeHeaders_Separately();
    }
    else
    {
        if($Descriptor{1}{"Path"} !~ /\.abi\.tar\.gz/)
        {
            parseHeaders_AllInOne(1);
        }
        if($Descriptor{2}{"Path"} !~ /\.abi\.tar\.gz/)
        {
            parseHeaders_AllInOne(2);
        }
        print "comparing headers ...\n";
        mergeSignatures();
        mergeConstants();
    }
    #libraries merging
    if(not $CheckHeadersOnly)
    {
        print "comparing shared objects ...\n";
        mergeLibs();
    }
    print "creating ABI compliance report ...\n";
    create_HtmlReport();
    if($HeaderCheckingMode_Separately)
    {
        if(keys(%HeaderCompileError))
        {
            print "\nWARNING: compilation errors in following headers:\n";
            foreach my $Header (keys(%HeaderCompileError))
            {
                print $Header." ";
            }
            print "\nyou can see compilation errors in the following files:\n  '$ERR_PATH{1}'\n  '$ERR_PATH{2}'\n";
        }
    }
    `rm -fr temp`;
    print "\nstarted: $StartTime, finished: ".(localtime time)."\n" if($ShowExpendTime);
    print "see the ABI compliance report in the file '$REPORT_PATH/abi_compat_report.html'\n";
    exit(0);
}

scenario();
