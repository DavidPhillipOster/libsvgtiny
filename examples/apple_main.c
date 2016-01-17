/*
 * This file is an optional part of Libsvgtiny
 * Licensed under the MIT License,
 *                http://opensource.org/licenses/mit-license.php
 *
 * it demonstrates SVG file i/o
 *
 * Copyright 2016 by David Phillip Oster
 */

#include "svgtiny.h"
#include "svgtiny_writer.h"
#include "svgtiny_report_err.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static char *test = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
"<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n"
"\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n"
"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"612px\" height=\"792px\" viewbox=\"0 0 612 792\">\n"
"<defs>\n"
"    <linearGradient id=\"grad1\" x1=\"0%\" y1=\"0%\" x2=\"100%\" y2=\"0%\">\n"
"      <stop offset=\"0%\" style=\"stop-color:rgb(255,255,0);stop-opacity:0.1\" />\n"
"      <stop offset=\"100%\" style=\"stop-color:rgb(255,0,0);stop-opacity:0.9\" />\n"
"    </linearGradient>\n"
"  </defs>\n"
"<rect x=\"27.3047\" y=\"29.3438\" width=\"75\" height=\"55\" style=\"fill: none; stroke: #000000; stroke-width:1 \"/>\n"
"<!-- <ellipse cx=\"200\" cy=\"70\" rx=\"85\" ry=\"55\" fill=\"url(#grad1)\" /> -->\n"
"<g>\n"
"<rect x=\"155.789\" y=\"32.6523\" width=\"53.3906\" height=\"75\" style=\"fill: #ff0103; stroke: #000000; stroke-width:6.10109 \"/>\n"
"<ellipse cx=\"68.625\" cy=\"144.387\" rx=\"29.5\" ry=\"32\" style=\"fill: none; stroke: #000000; stroke-width:1 \"/>\n"
"<ellipse cx=\"230.734\" cy=\"103.082\" rx=\"48.3477\" ry=\"29\" style=\"fill: #00ff00; fill-opacity:0.6; stroke: #000000; stroke-width:5 \"/>\n"
"<line x1=\"67.1143\" y1=\"84.7393\" x2=\"69.874\" y2=\"112.989\" style=\"fill: none; stroke: #000000; stroke-width:1 \"/>\n"
"<line x1=\"102.309\" y1=\"29.6416\" x2=\"153.61\" y2=\"29.8916\" style=\"fill: none; stroke: #000000; stroke-width:1 \"/>\n"
"<line x1=\"101.784\" y1=\"84.3154\" x2=\"153.927\" y2=\"110.75\" style=\"fill: none; stroke: #000000; stroke-width:9 \"/>\n"
"<polyline style=\"fill: none; stroke: #000000; stroke-width:8\" points=\"100,100 200,200, 150,150\"/>\n"

"</g>\n"
"</svg>\n";

// caller must free!
static char *diagramAsSVG(struct svgtiny_diagram *diagram) {
  char *result = NULL;
  if (diagram) {
    int bufferSize;
    svgtinywriter_code err = svgtinywriter_length(diagram, &bufferSize);
    if (svgtinywriter_OK == err) {
      result = malloc(bufferSize);
      if (result) {
        if(svgtinywriter_OK != (err = svgtinywriter_write(diagram, bufferSize, result, NULL))) {
          fprintf(stderr, "diagramAsSVG err:%d\n", err);
          free(result);
          result = NULL;
        }
      }
    }
  }
  return result;
}

static void test1() {
  struct svgtiny_diagram *diagram = svgtiny_create();
  if (diagram) {
    svgtiny_code code = svgtiny_parse(diagram, test, strlen(test), "http://dontcare", 612, 792);
    if (svgtiny_OK == code) {
      char *buffer = diagramAsSVG(diagram);
      if (buffer) {
        printf("%s\n\n\n", buffer);
        svgtiny_free(diagram);
        diagram = svgtiny_create();
        code = svgtiny_parse(diagram, buffer, strlen(buffer), "http://dontcare", 612, 792);
        if (svgtiny_OK == code) {
          char *buffer2 = diagramAsSVG(diagram);
          if (buffer2) {
            printf("%s", buffer2);
          }
        }
        free(buffer);
      }
    } else {
      svgtiny_report_err(code, diagram);
    }
    svgtiny_free(diagram);
  }
}

void testOfFile(const char *filename) {
  struct stat fileInfo;
  if (0 == stat(filename, &fileInfo)) {
    char *buffer = malloc(fileInfo.st_size);
    FILE *f = fopen(filename, "r");
    if (f) {
      size_t amount = fread(buffer, 1, fileInfo.st_size, f);
      fclose(f);
      if (amount == fileInfo.st_size) {
        struct svgtiny_diagram *diagram = svgtiny_create();
        if (diagram) {
          svgtiny_code code = svgtiny_parse(diagram, buffer, amount, "http://dontcare", 900, 900);
          svgtiny_report_err(code, diagram);
          svgtiny_free(diagram);
        }
      }
    }
  }
}


int main(int argc, const char * argv[]) {
  
  if (2 == argc) {
    testOfFile(argv[1]);
  } else {
    test1();
  }
  return 0;
}
