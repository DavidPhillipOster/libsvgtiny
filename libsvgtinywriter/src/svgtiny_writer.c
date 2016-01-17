#include "svgtiny_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "svgtiny.h"

svgtinywriter_code svgtinywriter_length(const struct svgtiny_diagram* diagram, int *outLength)
{
  svgtinywriter_code errCode = svgtinywriter_write(diagram, 1000000000, NULL,  outLength);
  if (errCode == svgtinywriter_OK && outLength) {
    *outLength += 2;
  }
  return errCode;
}

svgtinywriter_code svg_append(const char *s, int *maxLen, int *consumed, char **outBufferp)
{
  size_t len = strlen(s);
  if (*maxLen <= len) {
    return svgtinywriter_BUFFER_TOO_SMALL;
  }
  *maxLen -= len;
  *consumed += len;
  if (*outBufferp) {
    strcpy(*outBufferp, s);
    *outBufferp += len;
  }
  return svgtinywriter_OK;
}

// Caution! Caller is responsible for freeing the result.
static char *svg_quotetext(const char *text)
{
  int lengthK = (int)strlen(text);
  char *result = malloc(lengthK + 1);
  if (NULL == result) {
    return NULL;
  }
  int i = 0; int k = 0;
  char c;
  for (;'\0' != (c = text[i]); i++) {
    switch (c) {
    case '&':
      lengthK += 4;
      result = realloc(result, lengthK + 1);
      if (NULL == result) {
        return NULL;
      }
      result[k++] = '&';
      result[k++] = 'a';
      result[k++] = 'm';
      result[k++] = 'p';
      result[k++] = ';';
      break;
    case '<':
      lengthK += 3;
      result = realloc(result, lengthK + 1);
      if (NULL == result) {
        return NULL;
      }
      result[k++] = '&';
      result[k++] = 'l';
      result[k++] = 't';
      result[k++] = ';';
      break;
    case '>':
      lengthK += 3;
      result = realloc(result, lengthK + 1);
      if (NULL == result) {
        return NULL;
      }
      result[k++] = '&';
      result[k++] = 'g';
      result[k++] = 't';
      result[k++] = ';';
      break;
    default:
      result[k++] = c;
      break;
    }
  }
  result[k++] = '\0';
  return result;
}

static int IsTransparent(svgtiny_colour c) {
  if (c == svgtiny_TRANSPARENT) {
    return  1;
  }
  if (0 == svgtiny_ALPHA(c)) {
    return  1;
  }
  return 0;
}

static char *svg_style(char *buffer, int bufferlen, struct svgtiny_shape *shape)
{
  char fillBuffer[60];
  if ( ! (IsTransparent(shape->fill) || shape->fill == svgtiny_LINEAR_GRADIENT)) {
    char alphaBuffer[30];
    alphaBuffer[0] = '\0';
    if (0xff != svgtiny_ALPHA(shape->fill)) {
      snprintf(alphaBuffer, sizeof(alphaBuffer), "fill-opacity=\"%.6g\" ", svgtiny_ALPHA(shape->fill) / 255.0);
    }
    snprintf(fillBuffer, sizeof(fillBuffer), "fill=\"#%02x%02x%02x\" %s",
        svgtiny_RED(shape->fill), svgtiny_GREEN(shape->fill), svgtiny_BLUE(shape->fill), alphaBuffer);
  } else {
    fillBuffer[0] = '\0';
  }
  char strokeBuffer[80];
  if ( ! (IsTransparent(shape->stroke) || shape->stroke == svgtiny_LINEAR_GRADIENT)) {
    char alphaBuffer[30];
    alphaBuffer[0] = '\0';
    if (0xff != svgtiny_ALPHA(shape->stroke)) {
      snprintf(alphaBuffer, sizeof(alphaBuffer), "stroke-opacity=\"%.6g\" ", svgtiny_ALPHA(shape->fill) / 255.0);
    }
    snprintf(strokeBuffer, sizeof(strokeBuffer), "stroke=\"#%02x%02x%02x\" stroke-width=\"%.6g\" %s",
        svgtiny_RED(shape->stroke), svgtiny_GREEN(shape->stroke), svgtiny_BLUE(shape->stroke), (float)shape->stroke_width, alphaBuffer);
  } else {
    strokeBuffer[0] = '\0';
  }

  snprintf(buffer, bufferlen, "%s%s", fillBuffer, strokeBuffer);
  return buffer;
}

svgtinywriter_code svg_append_path(struct svgtiny_shape *shape, int *maxLen, int *consumed, char **outBufferp)
{
  svgtinywriter_code errCode = svgtinywriter_OK;
  if (svgtinywriter_OK == errCode) { errCode = svg_append("<path ", maxLen, consumed, outBufferp); }
  if (svgtinywriter_OK == errCode) {
    char styleBuffer[200];
    errCode = svg_append(svg_style(styleBuffer, sizeof styleBuffer, shape), maxLen, consumed, outBufferp);
  }
  if (svgtinywriter_OK == errCode) { errCode = svg_append("d=\"", maxLen, consumed, outBufferp); }
  if (svgtinywriter_OK == errCode && shape->path) {
    unsigned int i = 0;
    float *path = shape->path;
    while (i < shape->path_length && svgtinywriter_OK == errCode) {
      char partBuffer[60];
      int path_part_kind = (int)path[i];
      switch (path_part_kind) {
      case svgtiny_PATH_MOVE:
        snprintf(partBuffer, sizeof(partBuffer), "M %.6g %.6g ", path[i+1], path[i+2]);
        errCode = svg_append(partBuffer, maxLen, consumed, outBufferp);
        i += 3;
        break;
      case svgtiny_PATH_CLOSE:
        errCode = svg_append("Z ", maxLen, consumed, outBufferp);
        i += 1;
        break;
      case svgtiny_PATH_LINE:
        snprintf(partBuffer, sizeof(partBuffer), "L %.6g %.6g ", path[i+1], path[i+2]);
        errCode = svg_append(partBuffer, maxLen, consumed, outBufferp);
        i += 3;
        break;
      case svgtiny_PATH_BEZIER:
        snprintf(partBuffer, sizeof(partBuffer), "C %.6g %.6g %.6g %.6g %.6g %.6g ",
          path[i+1], path[i+2], path[i+3], path[i+4], path[i+5], path[i+6]);
        errCode = svg_append(partBuffer, maxLen, consumed, outBufferp);
        i += 7;
        break;
      default:
        errCode = svgtinywriter_SVG_ERROR;
      }
    }
  }
  if (svgtinywriter_OK == errCode) { errCode = svg_append("\"/>\n", maxLen, consumed, outBufferp); }
  return errCode;
}

svgtinywriter_code svg_append_text(struct svgtiny_shape *shape, int *maxLen, int *consumed, char **outBufferp)
{
  char buffer[1000];
  char styleBuffer[200];
  char *quotedText = svg_quotetext(shape->text);
  int count = snprintf(buffer, sizeof(buffer), "<text x=\"%.6g\" y=\"%.6g\" %s>%s</text>\n",
    shape->text_x, shape->text_y, svg_style(styleBuffer, sizeof styleBuffer, shape), quotedText);
  free(quotedText);
  if (sizeof(buffer) <= count) {
    return svgtinywriter_BUFFER_TOO_SMALL;
  }
  return svg_append(buffer, maxLen, consumed, outBufferp);
}

svgtinywriter_code svg_append_shape(struct svgtiny_shape *shape, int *maxLen, int *consumed, char **outBufferp)
{
  svgtinywriter_code errCode = svgtinywriter_SVG_ERROR;

  if (shape->path) {
    errCode = svg_append_path(shape, maxLen, consumed, outBufferp);
  } else if (shape->text) {
    errCode = svg_append_text(shape, maxLen, consumed, outBufferp);
  }
  return errCode;
}

svgtinywriter_code svgtinywriter_write(const struct svgtiny_diagram* diagram, int maximumLength, char *outputBuffer, int *outLength)
{
  svgtinywriter_code errCode = svgtinywriter_OK;
  char buffer[1000];
  int consumed = 0;
  int count = snprintf(buffer, sizeof(buffer), "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
"<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n"
"\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n"
"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%dpx\" height=\"%dpx\" viewbox=\"0 0 %d %d\">\n",
  diagram->width, diagram->height, diagram->width, diagram->height);
  if (sizeof(buffer) <= count) {
    return svgtinywriter_BUFFER_TOO_SMALL;
  }
  if (svgtinywriter_OK == errCode) { errCode = svg_append(buffer, &maximumLength, &consumed, &outputBuffer); }
  if (svgtinywriter_OK == errCode) {
    int count = (int)diagram->shape_count;
    for (int i = 0; i < count && svgtinywriter_OK == errCode; ++i) {
      errCode = svg_append_shape(&diagram->shape[i], &maximumLength, &consumed, &outputBuffer);
    }
  }
  if (svgtinywriter_OK == errCode) { errCode = svg_append("</svg>\n", &maximumLength, &consumed, &outputBuffer); }
  if (svgtinywriter_OK == errCode && outLength) {
    *outLength = consumed;
  }
  return errCode;
}

