/*
 * Copyright (c) 2019 Anton Stiopin astiopin@gmail.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <locale>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/gl.h>

#include "float2.h"
#include "args_parser.h"
#include "sdf_gl.h"
#include "sdf_atlas.h"
#include "glyph_painter.h"
#include "font.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"

ArgsParser   args;
SdfGl        sdf_gl;
SdfAtlas     sdf_atlas;
Font         font;
GlyphPainter gp;

int          max_tex_size = 2048;
int          width = 1024;
int          height = 0;
int          row_height = 96;
int          border_size = 16;

std::string  font_filename;
std::string  txt_in_filename;
std::string  txt_out_filename;
F2           tex_size = F2( 1024, 1024 );

std::vector<uint32_t> unicode_values;


std::string help = R"(Program for generating signed distance field font atlas.
Given TTF file, generates PNG image and JSON with glyph rectangles and metrics.
Copyright: ©2019 Anton Stiopin, astiopin@gmail.com
License: MIT
Usage: sdf_atlas -f font_file.ttf [options]
Options:
    -h              this help
    -i 'filename'  input text file containing characters that need to be included in atlas
    -o 'filename'   output file name (without extension)
    -tw 'size'      atlas image width in pixels, default 1024
    -th 'size'      atlas image height in pixels (optional)
    -bs 'size'      SDF distance in pixels, default 16
    -rh 'size'      row height in pixels (without SDF border), default 96
Example:
    sdf_atlas -f Roboto-Regular.ttf -o roboto -tw 2048 -th 2048 -bs 22 -rh 70 -ur 31:126,0xA0:0xFF,0x400:0x4FF,0xFFFF
)";

void show_help( ArgsParser* ) {
    std::cout << help;
    exit( 0 );
}

void read_font_filename( ArgsParser* ap ) {
    font_filename = ap->word();
}

void read_txt_in_filename(ArgsParser* ap) {
    txt_in_filename = ap->word();
}

void read_txt_out_filename( ArgsParser* ap ) {
    txt_out_filename = ap->word();
}

void read_tex_width( ArgsParser *ap ) {
    errno = 0;
    width = strtol( ap->word().c_str(), nullptr, 0 );
    if ( errno != 0 || width <= 0 ) {
        std::cerr << "Error reading texture width." << std::endl;
        exit( 1 );
    }
    if ( width > max_tex_size ) {
        std::cerr << "Maximum texture size is " << max_tex_size << ". Clamping width." << std::endl;
        width = max_tex_size;
    }
};

void read_tex_height( ArgsParser *ap ) {
    errno = 0;
    height = strtol( ap->word().c_str(), nullptr, 0 );
    if ( errno != 0 || height <= 0 ) {
        std::cerr << "Error reading texture height." << std::endl;
        exit( 1 );
    }
    if ( height > max_tex_size ) {
        std::cout << "Height exceeds maximum texture size. Setting to " << max_tex_size << ".\n";
        height = max_tex_size;
    }
};

void read_row_height( ArgsParser *ap ) {
    errno = 0;
    row_height = strtol( ap->word().c_str(), nullptr, 0 );
    if ( errno != 0 || row_height <= 4 ) {
        std::cerr << "Error reading row height." << std::endl;
        exit( 1 );
    }
}

void read_border_size( ArgsParser *ap ) {
    errno = 0;
    border_size = strtol( ap->word().c_str(), nullptr, 0 );
    if ( errno != 0 || border_size <= 0 ) {
        std::cerr << "Error reading border size." << std::endl;
        exit( 1 );        
    }
}

void render() {
    glClearColor( 0.0, 0.0, 0.0, 0.0 );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

    F2 tex_size = F2( width, height );

    glViewport( 0, 0, width, height );
    sdf_gl.render_sdf( tex_size, gp.fp.vertices, gp.lp.vertices );
}



int main( int argc, char* argv[] ) {
    if ( argc == 1 ) {
        std::cout << help;
        exit( 0 );
    }
    
    if ( !glfwInit() ) {
        std::cerr << "GLFW initailization error" << std::endl;
        exit( 1 );
    }
                           
    glfwWindowHint( GLFW_VISIBLE, GL_FALSE );
    GLFWwindow *window = glfwCreateWindow( 1, 1, "sdf_atlas", nullptr, nullptr );
    if ( !window ) {
        std::cerr << "GLFW error creating window" << std::endl;
        glfwTerminate();
        exit( 1 );
    }

    glfwSetWindowSize( window, 640, 480 );
    glfwMakeContextCurrent( window );

	GLenum err = glewInit();
    if ( err != GLEW_OK ) {
        std::cerr << "GLEW init error: " << glewGetErrorString( err ) << std::endl;
        exit( 1 );
    }

    // Reading command line parameters

    glGetIntegerv( GL_MAX_RENDERBUFFER_SIZE, &max_tex_size );

    args.commands["-h"]  = show_help;    
    args.commands["-f"]  = read_font_filename;
    args.commands["-i"]  = read_txt_in_filename;
    args.commands["-o"]  = read_txt_out_filename;
    args.commands["-tw"] = read_tex_width;
    args.commands["-th"] = read_tex_height;
    args.commands["-bs"] = read_border_size;
    args.commands["-rh"] = read_row_height;
    args.run( argc, argv );

    if ( font_filename.empty() ) {
        std::cerr << "Input file not specified" << std::endl;
        exit( 1 );
    }

    if ( !txt_in_filename.empty() ) {
        std::wifstream inputFile(txt_in_filename);
        if (inputFile) {
            inputFile.imbue(std::locale("en_US.UTF-8"));
            wchar_t ch;
            while (inputFile.get(ch)) {
                unicode_values.push_back(ch);
            }
            inputFile.close();
            std::sort(unicode_values.begin(), unicode_values.end());
        }
    }

    if ( txt_out_filename.empty() ) {
        size_t ext_dot = font_filename.find_last_of( "." );
        if ( ext_dot == std::string::npos ) {
            txt_out_filename = font_filename;
        } else {
            txt_out_filename = font_filename.substr( 0, ext_dot );
        }
    }

    if ( !font.load_ttf_file( font_filename.c_str() ) ) {
        std::cerr << "Error reading TTF file '" << font_filename << "' " << std::endl;
        exit( 1 );
    }

    // Allocating glyph rects

    sdf_atlas.init( &font, width, row_height, border_size );

    if ( unicode_values.empty() ) {
        sdf_atlas.allocate_unicode_range( 0x21, 0x7e );
        sdf_atlas.allocate_unicode_range( 0xffff, 0xffff );
    } else {
        for ( uint32_t v : unicode_values ) {
            sdf_atlas.allocate_codepoint(v);
        }
    }
    
    sdf_atlas.draw_glyphs( gp );

    std::cout << "Allocated " << sdf_atlas.glyph_count << " glyphs" << std::endl;
    std::cout << "Atlas maximum height is " << sdf_atlas.max_height << std::endl;

    if ( height == 0 ) {
        height = sdf_atlas.max_height;
    }

    uint8_t* picbuf = (uint8_t*) malloc( width * height );

    // GL initialization
    
    sdf_gl.init();    

    GLuint rbcolor;
    glGenRenderbuffers( 1, &rbcolor );
    glBindRenderbuffer( GL_RENDERBUFFER, rbcolor );
    glRenderbufferStorage( GL_RENDERBUFFER, GL_RED, width, height );
    glBindRenderbuffer( GL_RENDERBUFFER, 0 );

    GLuint rbds;
    glGenRenderbuffers( 1, &rbds );
    glBindRenderbuffer( GL_RENDERBUFFER, rbds );
    glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_STENCIL, width, height );
    glBindRenderbuffer( GL_RENDERBUFFER, 0 );

    GLuint fbo;
    glGenFramebuffers( 1, &fbo );
    glBindFramebuffer( GL_FRAMEBUFFER, fbo );
    glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbcolor );
    glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbds );

    if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
        std::cerr << "Error creating framebuffer!" << std::endl;
        exit( 1 );
    }

    // Rendering glyphs

    glViewport( 0, 0, width, height );
    glClearColor( 0.0, 0.0, 0.0, 0.0 );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
    sdf_gl.render_sdf( F2( width, height ), gp.fp.vertices, gp.lp.vertices );

    glReadPixels( 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, picbuf );

    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    glFinish();

    // Flipping the picture vertically

    uint8_t *row_swap = (uint8_t*) malloc( width );

    for ( int iy = 0; iy < height / 2; ++iy ) {
        uint8_t* row0 = picbuf + iy * width;
        uint8_t* row1 = picbuf + ( height - 1 - iy ) * width;
        memcpy( row_swap, row0, width );
        memcpy( row0, row1, width );
        memcpy( row1, row_swap, width );
    }

    free( row_swap );

    // Saving the picture

    std::string png_filename = txt_out_filename + ".png";
    if ( !stbi_write_png( png_filename.c_str(), width, height, 1, picbuf, width ) ) {
        std::cout << "Error writing png file." << std::endl;
        exit( 1 );
    }

    free( picbuf );

    // Saving JSON

    std::string json = sdf_atlas.json( height );
    std::ofstream json_file;
    json_file.open( txt_out_filename + ".js" );
    if ( !json_file ) {
        std::cout << "Error writing json file." << std::endl;
    }
    json_file << json;
    json_file.close();
    
    glfwTerminate();
    
    return 0;
}
