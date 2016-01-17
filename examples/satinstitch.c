/*
 * This file is an optional part of Libsvgtiny
 * Licensed under the MIT License,
 *                http://opensource.org/licenses/mit-license.php
 *
 * it demonstrates SVG file i/o
 *
 * Copyright 2016 by David Phillip Oster
 */
#import "svgtiny.h"
#include "svgtiny_writer.h"
#include "svgtiny_report_err.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static svgtiny_code SatinStitchOfShape(struct svgtiny_shape *shape) {
  svgtiny_code err = svgtiny_OK;
// TODO: path is a malloc'ed buffer of floats representing path parts.
// expand into array of parts.
// if there are closed quadrilaterals or closed triangles replace them by a polyline that is the satin stitch.
// collapse from array of parts back to a path.
  return err;
}

static svgtiny_code SatinStitchOfDiagram(struct svgtiny_diagram *diagram) {
  svgtiny_code err = svgtiny_OK;
  int count = (int)diagram->shape_count;
  for (int i = 0; i < count && svgtiny_OK == err; ++i) {
    struct svgtiny_shape *shape = &diagram->shape[i];
    if (shape->path) {
      err = SatinStitchOfShape(shape);
    }
  }
  return err;
}

static char *DiagramAsSVG(struct svgtiny_diagram *diagram) {
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


static int SatinStitchOfString(const char *svgStr, size_t len) {
  struct svgtiny_diagram *diagram = svgtiny_create();
  svgtiny_code code = svgtiny_SVG_ERROR;
  if (diagram) {
    svgtiny_code code = svgtiny_parse0(diagram, svgStr, len);
    svgtiny_report_err(code, diagram);
    if (svgtiny_OK == code) {
      code = SatinStitchOfDiagram(diagram);
      if (svgtiny_OK == code) {
        char *buffer = DiagramAsSVG(diagram);
        if (buffer) {
          printf("%s", buffer);
          free(buffer);
        }
      }
    }
    svgtiny_free(diagram);
  }
  return code == svgtiny_OK ? 0 : 1; // good.
}

// Expanding buffer as necessary returns true for good.
static int AppendCharToBuffer(char c, char **bufferp, int *consumedp, int *currentBufferSizep) {
  int consumed = *consumedp;
  int currentBufferSize = *currentBufferSizep;
  char *buffer = *bufferp;
  if (currentBufferSize <= consumed) {
    currentBufferSize += 10000;
    buffer = realloc(buffer, currentBufferSize);
    if (NULL == buffer) {
      return 0; // bad
    }
  }
  if (consumed < currentBufferSize) {
    buffer[consumed++] = c;
  }
  *consumedp = consumed;
  *currentBufferSizep = currentBufferSize;
  *bufferp = buffer;
  return 1; // good
}

static char *StringOfStudio() {
  int currentBufferSize = 10000;
  int consumed = 0;
  char *buffer = malloc(currentBufferSize);
  int c;
  while (EOF != (c = getchar())) {
    if ( ! AppendCharToBuffer(c, &buffer, &consumed, &currentBufferSize)) {
      return NULL;
    }
  }
  if ( ! AppendCharToBuffer('\0', &buffer, &consumed, &currentBufferSize)) {
    return NULL;
  }
  return buffer;
}


static int SatinStitchOfStdio() {
  int value = 1;
  char *buffer = StringOfStudio();
  if (buffer) {
    value = SatinStitchOfString(buffer, strlen(buffer));
    free(buffer);
  }
  return value;
}

static int SatinStitchOfFile(const char *filename) {
  int value = 1;
  struct stat fileInfo;
  if (0 == stat(filename, &fileInfo)) {
    char *buffer = malloc(fileInfo.st_size+1);
    if (buffer) {
      FILE *f = fopen(filename, "r");
      if (f) {
        size_t amount = fread(buffer, 1, fileInfo.st_size, f);
        fclose(f);
        if (amount == fileInfo.st_size) {
          value = SatinStitchOfString(buffer, amount);
        }
      }
      free(buffer);
    }
  } else {
    fprintf(stderr, "%s not found\n", filename);
  }
  return value;
}

static int Usage() {
  fprintf(stderr, "satinstich [ inputFile ] - copies SVG input to SVG atandard output, converting "
    "quadilateral and triangular paths to polylines suitable for input to emmboridermodder2\n");
  return 1;
}


int main(int argc, const char * argv[]) {
  int value = 0;
  switch (argc) {
  case 1:  value = SatinStitchOfStdio(); break;
  case 2:  value = SatinStitchOfFile(argv[1]); break;
  default: value = Usage(); break;
  }
  return value;
}
