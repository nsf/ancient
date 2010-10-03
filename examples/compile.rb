#!/usr/bin/env ruby

if ARGV.length == 2 then
	anc = ARGV[0]
	out = ARGV[1]
	
	sdlcflags = %x[sdl-config --cflags].strip
	sdllibs = %x[sdl-config --libs].strip

	%x[../ancient < #{anc}]
	%x[llc out.bc]
	%x[gcc -o #{out} #{sdlcflags} #{sdllibs} runtime.c out.s]
	%x[rm -rf out.bc out.s]
else
	puts "./compile.rb ANC OUT"
end
