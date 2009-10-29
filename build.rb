#!/usr/bin/env ruby -wKU

###################################################################
# Build Jamoma
###################################################################

# First include the functions in the jamoma lib
libdir = "."
Dir.chdir libdir             # change to libdir so that requires work
libdir = Dir.pwd
require "jamomalib"

if(ARGV.length == 0)
  puts "usage: "
  puts "build.rb <required:configuration> <optional:clean>"
  exit 0;
end

configuration = ARGV[0];
if win32?
 	 if(configuration == "Development" || configuration == "Debug" )
    		configuration = "Debug"
  	else
		if(configuration == "Deployment" || configuration == "Release" )
    			configuration = "Release"
		end
 	 end
end

clean = false;
@debug = false;

if(ARGV.length > 1)
  if(ARGV[1] != "0" && ARGV[1] != "false" && ARGV[1] != false)
    clean = true;
  end
end

if(ARGV.length > 2)
  if(ARGV[2] != "0" && ARGV[2] != "false" && ARGV[2] != false)
    @debug = true;
  end
end

version = nil
revision = nil


###################################################################
# Get Revision Info
###################################################################

git_desc = `cd ..; git describe --tags --abbrev=5 --long`.split('-')
git_tag = git_desc[0]
git_dirty_commits = git_desc[git_desc.size()-2]
git_rev = git_desc[git_desc.size()-1]
git_rev.sub!('g', '')
git_rev.chop!

version_digits = git_tag.split(/\./)
version_maj = 0
version_min = 0
version_sub = 0
version_mod = ''
version_mod = version_digits[3] if version_digits.size() > 3
version_sub = version_digits[2] if version_digits.size() > 2
version_min = version_digits[1] if version_digits.size() > 1
version_maj = version_digits[0] if version_digits.size() > 0

#puts ""
#puts "  Building Jamoma #{git_tag} (rev. #{git_rev})"
#puts ""
#if git_dirty_commits != '0'
#	puts "  !!! WARNING !!!"
#	puts "	THIS BUILD IS COMING FROM A DIRTY REVISION   "
#	puts "	THIS BUILD IS FOR PERSONAL USE ONLY  "
#	puts "	DO NOT DISTRIBUTE THIS BUILD TO OTHERS       "
#	puts ""
#end
#puts ""


if(ARGV.length > 3)
  version = ARGV[3]
else
  version = "#{version_maj}.#{version_min}.#{version_sub}-#{version_mod}"
end
if(ARGV.length > 4)
  revision = ARGV[4]
else
  revision = "#{git_rev}"
end

projectNameParts = libdir.split('/')
@projectName = projectNameParts[projectNameParts.size-2];


puts "Building Jamoma #{@projectName}"
puts "==================================================="
puts "  configuration: #{configuration}"
puts "  clean:   #{clean}"
#puts "  debug the build script: #{debug}"
puts "  version: #{version}"
puts "  rev:     #{revision} #{'            DIRTY REVISION' if git_dirty_commits != '0'}"
puts "  "


@log_root = "./logs"
@svn_root = "../"
@fail_array = Array.new
@zerolink = false



# Create the Shared XCConfig from the template we store in Git
file_path = "#{libdir}/max/tt-max.xcconfig"
`cp "#{libdir}/max/tt-max-template.xcconfig" "#{file_path}"`

if FileTest.exist?(file_path)
  f = File.open("#{file_path}", "r+")
  str = f.read

  if (version_mod == '' || version_mod.match(/rc(.*)/))
    str.sub!(/PRODUCT_VERSION = (.*)/, "PRODUCT_VERSION = #{version_maj}.#{version_min}.#{version_sub}")
  else
    str.sub!(/PRODUCT_VERSION = (.*)/, "PRODUCT_VERSION = #{version_maj}.#{version_min}.#{version_sub}#{version_mod}")
  end
  str.sub!(/SVNREV = (.*)/, "SVNREV = #{git_rev}")

  f.rewind
  f.write(str)
  f.truncate(f.pos)
  f.close
end



###################################################################
# CREATE LOG FILES AND RESET COUNTERS
###################################################################
create_logs
zero_count


###################################################################
# FRAMEWORK
###################################################################
puts "Building Frameworks..."
zero_count

if win32?
	build_project("#{@svn_root}/library", "Jamoma#{@projectName}.vcproj", configuration, true)
else
	build_project("#{@svn_root}/library", "Jamoma#{@projectName}.xcodeproj", configuration, true)
end

ex_total, ex_count = get_count
puts ""

puts "Building Extensions..."
zero_count
build_dir("extensions", configuration, clean)  
ex_total, ex_count = get_count
puts ""


###################################################################
# EXTERNALS
###################################################################
puts "Building Max Externals..."

zero_count
build_dir("implementations/MaxMSP", configuration, clean)  
ex_total, ex_count = get_count

extension = ".mxo"
if win32?
	extension = ".mxe"
end

src_folder = "Build_Mac"
if win32?
	src_folder = "MaxMSP/builds"
end

dst_folder = "mac"
if win32?
	dst_folder = "windows"
end

puts ""


###################################################################
# FINISH UP
###################################################################

puts "=================DONE===================="
puts "\nFailed projects:" if @fail_array.length > 0
@fail_array.each do |loser|
  puts loser
end

###################################################################
# CLOSE LOG FILES
###################################################################
close_logs
puts ""
