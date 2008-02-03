/*
 * This file is part of Libsvgtiny
 * Licensed under the MIT License,
 *                http://opensource.org/licenses/mit-license.php
 * Copyright 2008 James Bursa <james@semichrome.net>
 */

#define _GNU_SOURCE  /* for strndup */
#include <assert.h>
#include <math.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/debugXML.h>
#include "svgtiny.h"


struct svgtiny_parse_state {
	struct svgtiny_diagram *diagram;
	xmlDoc *document;

	float viewport_width;
	float viewport_height;

	/* current transformation matrix */
	struct {
		float a, b, c, d, e, f;
	} ctm;

	/*struct css_style style;*/

	/* paint attributes */
	svgtiny_colour fill;
	svgtiny_colour stroke;
	int stroke_width;
};


static bool svgtiny_parse_svg(xmlNode *svg, struct svgtiny_parse_state state);
static bool svgtiny_parse_path(xmlNode *path, struct svgtiny_parse_state state);
static bool svgtiny_parse_rect(xmlNode *rect, struct svgtiny_parse_state state);
static bool svgtiny_parse_circle(xmlNode *circle,
		struct svgtiny_parse_state state);
static bool svgtiny_parse_line(xmlNode *line, struct svgtiny_parse_state state);
static bool svgtiny_parse_poly(xmlNode *poly, struct svgtiny_parse_state state,
		bool polygon);
static bool svgtiny_parse_text(xmlNode *text, struct svgtiny_parse_state state);
static void svgtiny_parse_position_attributes(const xmlNode *node,
		const struct svgtiny_parse_state state,
		float *x, float *y, float *width, float *height);
static float svgtiny_parse_length(const char *s, int viewport_size,
		const struct svgtiny_parse_state state);
static void svgtiny_parse_paint_attributes(const xmlNode *node,
		struct svgtiny_parse_state *state);
static void svgtiny_parse_color(const char *s, svgtiny_colour *c,
		struct svgtiny_parse_state *state);
static void svgtiny_parse_font_attributes(const xmlNode *node,
		struct svgtiny_parse_state *state);
static void svgtiny_parse_transform_attributes(xmlNode *node,
		struct svgtiny_parse_state *state);
static struct svgtiny_shape *svgtiny_add_shape(
		struct svgtiny_parse_state *state);
static void svgtiny_transform_path(float *p, unsigned int n,
		struct svgtiny_parse_state *state);


struct svgtiny_diagram *svgtiny_create(void)
{
	struct svgtiny_diagram *diagram;

	diagram = malloc(sizeof *diagram);
	if (!diagram)
		return 0;

	diagram->shape = 0;
	diagram->shape_count = 0;

	return diagram;
}


svgtiny_code svgtiny_parse(struct svgtiny_diagram *diagram,
		const char *buffer, size_t size, const char *url,
		int viewport_width, int viewport_height)
{
	xmlDoc *document;
	xmlNode *svg;
	struct svgtiny_parse_state state;

	assert(diagram);
	assert(buffer);
	assert(url);

	/* parse XML to tree */
	document = xmlReadMemory(buffer, size, url, 0,
			XML_PARSE_NONET | XML_PARSE_COMPACT |
			XML_PARSE_DTDVALID /* needed for xmlGetID to work */);
	if (!document)
		return svgtiny_LIBXML_ERROR;

	/*xmlDebugDumpDocument(stderr, document);*/

	/* find root <svg> element */
	svg = xmlDocGetRootElement(document);
	if (!svg)
		return svgtiny_NOT_SVG;
	if (strcmp((const char *) svg->name, "svg") != 0)
		return svgtiny_NOT_SVG;

	/* get graphic dimensions */
	float x, y, width, height;
	state.diagram = diagram;
	state.document = document;
	state.viewport_width = viewport_width;
	state.viewport_height = viewport_height;
	svgtiny_parse_position_attributes(svg, state, &x, &y, &width, &height);
	diagram->width = width;
	diagram->height = height;

	state.viewport_width = width;
	state.viewport_height = height;
	state.ctm.a = 1; /*(float) viewport_width / (float) width;*/
	state.ctm.b = 0;
	state.ctm.c = 0;
	state.ctm.d = 1; /*(float) viewport_height / (float) height;*/
	state.ctm.e = 0; /*x;*/
	state.ctm.f = 0; /*y;*/
	/*state.style = css_base_style;
	state.style.font_size.value.length.value = option_font_size * 0.1;*/
	state.fill = 0x000000;
	state.stroke = svgtiny_TRANSPARENT;
	state.stroke_width = 1;

	svgtiny_parse_svg(svg, state);

	xmlFreeDoc(document);

	return svgtiny_OK;
}


/**
 * Parse a <svg> or <g> element node.
 */

bool svgtiny_parse_svg(xmlNode *svg, struct svgtiny_parse_state state)
{
	float x, y, width, height;

	svgtiny_parse_position_attributes(svg, state, &x, &y, &width, &height);
	svgtiny_parse_paint_attributes(svg, &state);
	svgtiny_parse_font_attributes(svg, &state);

	/* parse viewBox */
	xmlAttr *view_box = xmlHasProp(svg, (const xmlChar *) "viewBox");
	if (view_box) {
		const char *s = (const char *) view_box->children->content;
		float min_x, min_y, vwidth, vheight;
		if (sscanf(s, "%f,%f,%f,%f",
				&min_x, &min_y, &vwidth, &vheight) == 4 ||
				sscanf(s, "%f %f %f %f",
				&min_x, &min_y, &vwidth, &vheight) == 4) {
			state.ctm.a = (float) state.viewport_width / vwidth;
			state.ctm.d = (float) state.viewport_height / vheight;
			state.ctm.e += -min_x * state.ctm.a;
			state.ctm.f += -min_y * state.ctm.d;
		}
	}

	svgtiny_parse_transform_attributes(svg, &state);

	for (xmlNode *child = svg->children; child; child = child->next) {
		bool ok = true;

		if (child->type == XML_ELEMENT_NODE) {
			const char *name = (const char *) child->name;
			if (strcmp(name, "svg") == 0)
				ok = svgtiny_parse_svg(child, state);
			else if (strcmp(name, "g") == 0)
				ok = svgtiny_parse_svg(child, state);
			else if (strcmp(name, "a") == 0)
				ok = svgtiny_parse_svg(child, state);
			else if (strcmp(name, "path") == 0)
				ok = svgtiny_parse_path(child, state);
			else if (strcmp(name, "rect") == 0)
				ok = svgtiny_parse_rect(child, state);
			else if (strcmp(name, "circle") == 0)
				ok = svgtiny_parse_circle(child, state);
			else if (strcmp(name, "line") == 0)
				ok = svgtiny_parse_line(child, state);
			else if (strcmp(name, "polyline") == 0)
				ok = svgtiny_parse_poly(child, state, false);
			else if (strcmp(name, "polygon") == 0)
				ok = svgtiny_parse_poly(child, state, true);
			else if (strcmp(name, "text") == 0)
				ok = svgtiny_parse_text(child, state);
		}

		if (!ok)
			return false;
	}

	return true;
}


/**
 * Parse a <path> element node.
 *
 * http://www.w3.org/TR/SVG11/paths#PathElement
 */

bool svgtiny_parse_path(xmlNode *path, struct svgtiny_parse_state state)
{
	char *s, *path_d;

	svgtiny_parse_paint_attributes(path, &state);
	svgtiny_parse_transform_attributes(path, &state);

	/* read d attribute */
	s = path_d = (char *) xmlGetProp(path, (const xmlChar *) "d");
	if (!s) {
		/*LOG(("path missing d attribute"));*/
		return false;
	}

	/* allocate space for path: it will never have more elements than d */
	float *p = malloc(sizeof p[0] * strlen(s));
	if (!p) {
		/*LOG(("out of memory"));*/
		return false;
	}

	/* parse d and build path */
	for (unsigned int i = 0; s[i]; i++)
		if (s[i] == ',')
			s[i] = ' ';
	unsigned int i = 0;
	float last_x = 0, last_y = 0;
	float last_cubic_x = 0, last_cubic_y = 0;
	float last_quad_x = 0, last_quad_y = 0;
	while (*s) {
		char command[2];
		int plot_command;
		float x, y, x1, y1, x2, y2;
		int n;

		/* moveto (M, m), lineto (L, l) (2 arguments) */
		if (sscanf(s, " %1[MmLl] %f %f %n", command, &x, &y, &n) == 3) {
			/*LOG(("moveto or lineto"));*/
			if (*command == 'M' || *command == 'm')
				plot_command = svgtiny_PATH_MOVE;
			else
				plot_command = svgtiny_PATH_LINE;
			do {
				p[i++] = plot_command;
				if ('a' <= *command) {
					x += last_x;
					y += last_y;
				}
				p[i++] = last_cubic_x = last_quad_x = last_x
						= x;
				p[i++] = last_cubic_y = last_quad_y = last_y
						= y;
				s += n;
				plot_command = svgtiny_PATH_LINE;
			} while (sscanf(s, "%f %f %n", &x, &y, &n) == 2);

		/* closepath (Z, z) (no arguments) */
		} else if (sscanf(s, " %1[Zz] %n", command, &n) == 1) {
			/*LOG(("closepath"));*/
			p[i++] = svgtiny_PATH_CLOSE;
			s += n;

		/* horizontal lineto (H, h) (1 argument) */
		} else if (sscanf(s, " %1[Hh] %f %n", command, &x, &n) == 2) {
			/*LOG(("horizontal lineto"));*/
			do {
				p[i++] = svgtiny_PATH_LINE;
				if (*command == 'h')
					x += last_x;
				p[i++] = last_cubic_x = last_quad_x = last_x
						= x;
				p[i++] = last_cubic_y = last_quad_y = last_y;
				s += n;
			} while (sscanf(s, "%f %n", &x, &n) == 1);

		/* vertical lineto (V, v) (1 argument) */
		} else if (sscanf(s, " %1[Vv] %f %n", command, &y, &n) == 2) {
			/*LOG(("vertical lineto"));*/
			do {
				p[i++] = svgtiny_PATH_LINE;
				if (*command == 'v')
					y += last_y;
				p[i++] = last_cubic_x = last_quad_x = last_x;
				p[i++] = last_cubic_y = last_quad_y = last_y
						= y;
				s += n;
			} while (sscanf(s, "%f %n", &x, &n) == 1);

		/* curveto (C, c) (6 arguments) */
		} else if (sscanf(s, " %1[Cc] %f %f %f %f %f %f %n", command,
				&x1, &y1, &x2, &y2, &x, &y, &n) == 7) {
			/*LOG(("curveto"));*/
			do {
				p[i++] = svgtiny_PATH_BEZIER;
				if (*command == 'c') {
					x1 += last_x;
					y1 += last_y;
					x2 += last_x;
					y2 += last_y;
					x += last_x;
					y += last_y;
				}
				p[i++] = x1;
				p[i++] = y1;
				p[i++] = last_cubic_x = x2;
				p[i++] = last_cubic_y = y2;
				p[i++] = last_quad_x = last_x = x;
				p[i++] = last_quad_y = last_y = y;
				s += n;
			} while (sscanf(s, "%f %f %f %f %f %f %n",
					&x1, &y1, &x2, &y2, &x, &y, &n) == 6);

		/* shorthand/smooth curveto (S, s) (4 arguments) */
		} else if (sscanf(s, " %1[Ss] %f %f %f %f %n", command,
				&x2, &y2, &x, &y, &n) == 5) {
			/*LOG(("shorthand/smooth curveto"));*/
			do {
				p[i++] = svgtiny_PATH_BEZIER;
				x1 = last_x + (last_x - last_cubic_x);
				y1 = last_y + (last_y - last_cubic_y);
				if (*command == 's') {
					x2 += last_x;
					y2 += last_y;
					x += last_x;
					y += last_y;
				}
				p[i++] = x1;
				p[i++] = y1;
				p[i++] = last_cubic_x = x2;
				p[i++] = last_cubic_y = y2;
				p[i++] = last_quad_x = last_x = x;
				p[i++] = last_quad_y = last_y = y;
				s += n;
			} while (sscanf(s, "%f %f %f %f %n",
					&x2, &y2, &x, &y, &n) == 4);

		/* quadratic Bezier curveto (Q, q) (4 arguments) */
		} else if (sscanf(s, " %1[Qq] %f %f %f %f %n", command,
				&x1, &y1, &x, &y, &n) == 5) {
			/*LOG(("quadratic Bezier curveto"));*/
			do {
				p[i++] = svgtiny_PATH_BEZIER;
				last_quad_x = x1;
				last_quad_y = y1;
				if (*command == 'q') {
					x1 += last_x;
					y1 += last_y;
					x += last_x;
					y += last_y;
				}
				p[i++] = 1./3 * last_x + 2./3 * x1;
				p[i++] = 1./3 * last_y + 2./3 * y1;
				p[i++] = 2./3 * x1 + 1./3 * x;
				p[i++] = 2./3 * y1 + 1./3 * y;
				p[i++] = last_cubic_x = last_x = x;
				p[i++] = last_cubic_y = last_y = y;
				s += n;
			} while (sscanf(s, "%f %f %f %f %n",
					&x1, &y1, &x, &y, &n) == 4);

		/* shorthand/smooth quadratic Bezier curveto (T, t)
		   (2 arguments) */
		} else if (sscanf(s, " %1[Tt] %f %f %n", command,
				&x, &y, &n) == 3) {
			/*LOG(("shorthand/smooth quadratic Bezier curveto"));*/
			do {
				p[i++] = svgtiny_PATH_BEZIER;
				x1 = last_x + (last_x - last_quad_x);
				y1 = last_y + (last_y - last_quad_y);
				last_quad_x = x1;
				last_quad_y = y1;
				if (*command == 't') {
					x1 += last_x;
					y1 += last_y;
					x += last_x;
					y += last_y;
				}
				p[i++] = 1./3 * last_x + 2./3 * x1;
				p[i++] = 1./3 * last_y + 2./3 * y1;
				p[i++] = 2./3 * x1 + 1./3 * x;
				p[i++] = 2./3 * y1 + 1./3 * y;
				p[i++] = last_cubic_x = last_x = x;
				p[i++] = last_cubic_y = last_y = y;
				s += n;
			} while (sscanf(s, "%f %f %n",
					&x, &y, &n) == 2);

		} else {
			/*LOG(("parse failed at \"%s\"", s));*/
			break;
		}
	}

	xmlFree(path_d);

	svgtiny_transform_path(p, i, &state);

	struct svgtiny_shape *shape = svgtiny_add_shape(&state);
	if (!shape) {
		free(p);
		return false;
	}
	shape->path = p;
	shape->path_length = i;
	state.diagram->shape_count++;

	return true;
}


/**
 * Parse a <rect> element node.
 *
 * http://www.w3.org/TR/SVG11/shapes#RectElement
 */

bool svgtiny_parse_rect(xmlNode *rect, struct svgtiny_parse_state state)
{
	float x, y, width, height;

	svgtiny_parse_position_attributes(rect, state,
			&x, &y, &width, &height);
	svgtiny_parse_paint_attributes(rect, &state);
	svgtiny_parse_transform_attributes(rect, &state);

	float *p = malloc(13 * sizeof p[0]);
	if (!p)
		return false;

	p[0] = svgtiny_PATH_MOVE;
	p[1] = x;
	p[2] = y;
	p[3] = svgtiny_PATH_LINE;
	p[4] = x + width;
	p[5] = y;
	p[6] = svgtiny_PATH_LINE;
	p[7] = x + width;
	p[8] = y + height;
	p[9] = svgtiny_PATH_LINE;
	p[10] = x;
	p[11] = y + height;
	p[12] = svgtiny_PATH_CLOSE;

	svgtiny_transform_path(p, 13, &state);

	struct svgtiny_shape *shape = svgtiny_add_shape(&state);
	if (!shape) {
		free(p);
		return false;
	}
	shape->path = p;
	shape->path_length = 13;
	state.diagram->shape_count++;

	return true;
}


/**
 * Parse a <circle> element node.
 */

bool svgtiny_parse_circle(xmlNode *circle, struct svgtiny_parse_state state)
{
	float x = 0, y = 0, r = 0;
	const float kappa = 0.5522847498;

	for (xmlAttr *attr = circle->properties; attr; attr = attr->next) {
		const char *name = (const char *) attr->name;
		const char *content = (const char *) attr->children->content;
		if (strcmp(name, "cx") == 0)
			x = svgtiny_parse_length(content,
					state.viewport_width, state);
		else if (strcmp(name, "cy") == 0)
			y = svgtiny_parse_length(content,
					state.viewport_height, state);
		else if (strcmp(name, "r") == 0)
			r = svgtiny_parse_length(content,
					state.viewport_width, state);
        }
	svgtiny_parse_paint_attributes(circle, &state);
	svgtiny_parse_transform_attributes(circle, &state);

	float *p = malloc(32 * sizeof p[0]);
	if (!p)
		return false;

	p[0] = svgtiny_PATH_MOVE;
	p[1] = x - r;
	p[2] = y;
	p[3] = svgtiny_PATH_BEZIER;
	p[4] = x - r;
	p[5] = y + r * kappa;
	p[6] = x - r * kappa;
	p[7] = y + r;
	p[8] = x;
	p[9] = y + r;
	p[10] = svgtiny_PATH_BEZIER;
	p[11] = x + r * kappa;
	p[12] = y + r;
	p[13] = x + r;
	p[14] = y + r * kappa;
	p[15] = x + r;
	p[16] = y;
	p[17] = svgtiny_PATH_BEZIER;
	p[18] = x + r;
	p[19] = y - r * kappa;
	p[20] = x + r * kappa;
	p[21] = y - r;
	p[22] = x;
	p[23] = y - r;
	p[24] = svgtiny_PATH_BEZIER;
	p[25] = x - r * kappa;
	p[26] = y - r;
	p[27] = x - r;
	p[28] = y - r * kappa;
	p[29] = x - r;
	p[30] = y;
	p[31] = svgtiny_PATH_CLOSE;
	
	svgtiny_transform_path(p, 32, &state);

	struct svgtiny_shape *shape = svgtiny_add_shape(&state);
	if (!shape) {
		free(p);
		return false;
	}
	shape->path = p;
	shape->path_length = 32;
	state.diagram->shape_count++;

	return true;
}


/**
 * Parse a <line> element node.
 */

bool svgtiny_parse_line(xmlNode *line, struct svgtiny_parse_state state)
{
	float x1 = 0, y1 = 0, x2 = 0, y2 = 0;

	for (xmlAttr *attr = line->properties; attr; attr = attr->next) {
		const char *name = (const char *) attr->name;
		const char *content = (const char *) attr->children->content;
		if (strcmp(name, "x1") == 0)
			x1 = svgtiny_parse_length(content,
					state.viewport_width, state);
		else if (strcmp(name, "y1") == 0)
			y1 = svgtiny_parse_length(content,
					state.viewport_height, state);
		else if (strcmp(name, "x2") == 0)
			x2 = svgtiny_parse_length(content,
					state.viewport_width, state);
		else if (strcmp(name, "y2") == 0)
			y2 = svgtiny_parse_length(content,
					state.viewport_height, state);
        }
	svgtiny_parse_paint_attributes(line, &state);
	svgtiny_parse_transform_attributes(line, &state);

	float *p = malloc(7 * sizeof p[0]);
	if (!p)
		return false;

	p[0] = svgtiny_PATH_MOVE;
	p[1] = x1;
	p[2] = y1;
	p[3] = svgtiny_PATH_LINE;
	p[4] = x2;
	p[5] = y2;
	p[6] = svgtiny_PATH_CLOSE;

	svgtiny_transform_path(p, 7, &state);

	struct svgtiny_shape *shape = svgtiny_add_shape(&state);
	if (!shape) {
		free(p);
		return false;
	}
	shape->path = p;
	shape->path_length = 7;
	state.diagram->shape_count++;

	return true;
}


/**
 * Parse a <polyline> or <polygon> element node.
 *
 * http://www.w3.org/TR/SVG11/shapes#PolylineElement
 * http://www.w3.org/TR/SVG11/shapes#PolygonElement
 */

bool svgtiny_parse_poly(xmlNode *poly, struct svgtiny_parse_state state,
		bool polygon)
{
	char *s, *points;

	svgtiny_parse_paint_attributes(poly, &state);
	svgtiny_parse_transform_attributes(poly, &state);

	/* read d attribute */
	s = points = (char *) xmlGetProp(poly, (const xmlChar *) "points");
	if (!s) {
		/*LOG(("poly missing d attribute"));*/
		return false;
	}

	/* allocate space for path: it will never have more elements than s */
	float *p = malloc(sizeof p[0] * strlen(s));
	if (!p) {
		/*LOG(("out of memory"));*/
		return false;
	}

	/* parse s and build path */
	for (unsigned int i = 0; s[i]; i++)
		if (s[i] == ',')
			s[i] = ' ';
	unsigned int i = 0;
	while (*s) {
		float x, y;
		int n;

		if (sscanf(s, "%f %f %n", &x, &y, &n) == 2) {
			if (i == 0)
				p[i++] = svgtiny_PATH_MOVE;
			else
				p[i++] = svgtiny_PATH_LINE;
			p[i++] = x;
			p[i++] = y;
			s += n;
                } else {
                	break;
                }
        }
        if (polygon)
		p[i++] = svgtiny_PATH_CLOSE;

	xmlFree(points);

	svgtiny_transform_path(p, i, &state);

	struct svgtiny_shape *shape = svgtiny_add_shape(&state);
	if (!shape) {
		free(p);
		return false;
	}
	shape->path = p;
	shape->path_length = i;
	state.diagram->shape_count++;

	return true;
}


/**
 * Parse a <text> or <tspan> element node.
 */

bool svgtiny_parse_text(xmlNode *text, struct svgtiny_parse_state state)
{
	float x, y, width, height;

	svgtiny_parse_position_attributes(text, state,
			&x, &y, &width, &height);
	svgtiny_parse_font_attributes(text, &state);
	svgtiny_parse_transform_attributes(text, &state);

	float px = state.ctm.a * x + state.ctm.c * y + state.ctm.e;
	float py = state.ctm.b * x + state.ctm.d * y + state.ctm.f;
/* 	state.ctm.e = px - state.origin_x; */
/* 	state.ctm.f = py - state.origin_y; */

	/*struct css_style style = state.style;
	style.font_size.value.length.value *= state.ctm.a;*/

	for (xmlNode *child = text->children; child; child = child->next) {
		bool ok = true;

		if (child->type == XML_TEXT_NODE) {
			struct svgtiny_shape *shape = svgtiny_add_shape(&state);
			if (!shape)
				return false;
			shape->text = strdup((const char *) child->content);
			shape->text_x = px;
			shape->text_y = py;
			state.diagram->shape_count++;

		} else if (child->type == XML_ELEMENT_NODE &&
				strcmp((const char *) child->name,
					"tspan") == 0) {
			ok = svgtiny_parse_text(child, state);
		}

		if (!ok)
			return false;
	}

	return true;
}


/**
 * Parse x, y, width, and height attributes, if present.
 */

void svgtiny_parse_position_attributes(const xmlNode *node,
		const struct svgtiny_parse_state state,
		float *x, float *y, float *width, float *height)
{
	*x = 0;
	*y = 0;
	*width = state.viewport_width;
	*height = state.viewport_height;

	for (xmlAttr *attr = node->properties; attr; attr = attr->next) {
		const char *name = (const char *) attr->name;
		const char *content = (const char *) attr->children->content;
		if (strcmp(name, "x") == 0)
			*x = svgtiny_parse_length(content,
					state.viewport_width, state);
		else if (strcmp(name, "y") == 0)
			*y = svgtiny_parse_length(content,
					state.viewport_height, state);
		else if (strcmp(name, "width") == 0)
			*width = svgtiny_parse_length(content,
					state.viewport_width, state);
		else if (strcmp(name, "height") == 0)
			*height = svgtiny_parse_length(content,
					state.viewport_height, state);
	}
}


/**
 * Parse a length as a number of pixels.
 */

float svgtiny_parse_length(const char *s, int viewport_size,
		const struct svgtiny_parse_state state)
{
	int num_length = strspn(s, "0123456789+-.");
	const char *unit = s + num_length;
	float n = atof((const char *) s);
	float font_size = 20; /*css_len2px(&state.style.font_size.value.length, 0);*/

	if (unit[0] == 0) {
		return n;
	} else if (unit[0] == '%') {
		return n / 100.0 * viewport_size;
	} else if (unit[0] == 'e' && unit[1] == 'm') {
		return n * font_size;
	} else if (unit[0] == 'e' && unit[1] == 'x') {
		return n / 2.0 * font_size;
	} else if (unit[0] == 'p' && unit[1] == 'x') {
		return n;
	} else if (unit[0] == 'p' && unit[1] == 't') {
		return n * 1.25;
	} else if (unit[0] == 'p' && unit[1] == 'c') {
		return n * 15.0;
	} else if (unit[0] == 'm' && unit[1] == 'm') {
		return n * 3.543307;
	} else if (unit[0] == 'c' && unit[1] == 'm') {
		return n * 35.43307;
	} else if (unit[0] == 'i' && unit[1] == 'n') {
		return n * 90;
	}

	return 0;
}


/**
 * Parse paint attributes, if present.
 */

void svgtiny_parse_paint_attributes(const xmlNode *node,
		struct svgtiny_parse_state *state)
{
	for (const xmlAttr *attr = node->properties; attr; attr = attr->next) {
		const char *name = (const char *) attr->name;
		const char *content = (const char *) attr->children->content;
		if (strcmp(name, "fill") == 0)
			svgtiny_parse_color(content, &state->fill, state);
		else if (strcmp(name, "stroke") == 0)
			svgtiny_parse_color(content, &state->stroke, state);
		else if (strcmp(name, "stroke-width") == 0)
			state->stroke_width = svgtiny_parse_length(content,
					state->viewport_width, *state);
		else if (strcmp(name, "style") == 0) {
			const char *style = (const char *)
					attr->children->content;
			const char *s;
			char *value;
			if ((s = strstr(style, "fill:"))) {
				s += 5;
				while (*s == ' ')
					s++;
				value = strndup(s, strcspn(s, "; "));
				svgtiny_parse_color(value, &state->fill, state);
				free(value);
			}
			if ((s = strstr(style, "stroke:"))) {
				s += 7;
				while (*s == ' ')
					s++;
				value = strndup(s, strcspn(s, "; "));
				svgtiny_parse_color(value, &state->stroke, state);
				free(value);
			}
			if ((s = strstr(style, "stroke-width:"))) {
				s += 13;
				while (*s == ' ')
					s++;
				state->stroke_width = svgtiny_parse_length(s,
						state->viewport_width, *state);
			}
		}
	}
}


/**
 * Parse a colour.
 */

void svgtiny_parse_color(const char *s, svgtiny_colour *c,
		struct svgtiny_parse_state *state)
{
	unsigned int r, g, b;
	float rf, gf, bf;
	size_t len = strlen(s);
	char *id = 0, *rparen;
	xmlAttr *id_attr;

	if (len == 4 && s[0] == '#') {
		if (sscanf(s + 1, "%1x%1x%1x", &r, &g, &b) == 3)
			*c = svgtiny_RGB(r | r << 4, g | g << 4, b | b << 4);

	} else if (len == 7 && s[0] == '#') {
		if (sscanf(s + 1, "%2x%2x%2x", &r, &g, &b) == 3)
			*c = svgtiny_RGB(r, g, b);

	} else if (10 <= len && s[0] == 'r' && s[1] == 'g' && s[2] == 'b' &&
			s[3] == '(' && s[len - 1] == ')') {
		if (sscanf(s + 4, "%i,%i,%i", &r, &g, &b) == 3)
			*c = svgtiny_RGB(r, g, b);
		else if (sscanf(s + 4, "%f%%,%f%%,%f%%", &rf, &gf, &bf) == 3) {
			b = bf * 255 / 100;
			g = gf * 255 / 100;
			r = rf * 255 / 100;
			*c = svgtiny_RGB(r, g, b);
		}

	} else if (len == 4 && strcmp(s, "none") == 0) {
		*c = svgtiny_TRANSPARENT;

	} else if (5 < len && s[0] == 'u' && s[1] == 'r' && s[2] == 'l' &&
			s[3] == '(') {
		if (s[4] == '#') {
			id = strdup(s + 5);
			if (!id)
				return;
			rparen = strchr(id, ')');
			if (rparen)
				*rparen = 0;
			id_attr = xmlGetID(state->document,
					(const xmlChar *) id);
			if (!id_attr) {
				fprintf(stderr, "id \"%s\" not found\n", id);
				free(id);
				return;
			}
			fprintf(stderr, "id \"%s\" at %p\n", id, id_attr);
			free(id);
		}

	} else {
		const struct svgtiny_named_color *named_color;
		named_color = svgtiny_color_lookup(s, strlen(s));
		if (named_color)
			*c = named_color->color;
	}
}


/**
 * Parse font attributes, if present.
 */

void svgtiny_parse_font_attributes(const xmlNode *node,
		struct svgtiny_parse_state *state)
{
	for (const xmlAttr *attr = node->properties; attr; attr = attr->next) {
		if (strcmp((const char *) attr->name, "font-size") == 0) {
			/*if (css_parse_length(
					(const char *) attr->children->content,
					&state->style.font_size.value.length,
					true, true)) {
				state->style.font_size.size =
						CSS_FONT_SIZE_LENGTH;
			}*/
		}
        }
}


/**
 * Parse transform attributes, if present.
 *
 * http://www.w3.org/TR/SVG11/coords#TransformAttribute
 */

void svgtiny_parse_transform_attributes(xmlNode *node,
		struct svgtiny_parse_state *state)
{
	char *transform, *s;
	float a, b, c, d, e, f;
	float ctm_a, ctm_b, ctm_c, ctm_d, ctm_e, ctm_f;
	float angle, x, y;
	int n;

	/* parse transform */
	s = transform = (char *) xmlGetProp(node,
			(const xmlChar *) "transform");
	if (transform) {
		for (unsigned int i = 0; transform[i]; i++)
			if (transform[i] == ',')
				transform[i] = ' ';

		while (*s) {
			a = d = 1;
			b = c = 0;
			e = f = 0;
			if (sscanf(s, "matrix (%f %f %f %f %f %f) %n",
					&a, &b, &c, &d, &e, &f, &n) == 6)
				;
			else if (sscanf(s, "translate (%f %f) %n",
					&e, &f, &n) == 2)
				;
			else if (sscanf(s, "translate (%f) %n",
					&e, &n) == 1)
				;
			else if (sscanf(s, "scale (%f %f) %n",
					&a, &d, &n) == 2)
				;
			else if (sscanf(s, "scale (%f) %n",
					&a, &n) == 1)
				d = a;
			else if (sscanf(s, "rotate (%f %f %f) %n",
					&angle, &x, &y, &n) == 3) {
				angle = angle / 180 * M_PI;
				a = cos(angle);
				b = sin(angle);
				c = -sin(angle);
				d = cos(angle);
				e = -x * cos(angle) + y * sin(angle) + x;
				f = -x * sin(angle) - y * cos(angle) + y;
	                } else if (sscanf(s, "rotate (%f) %n",
					&angle, &n) == 1) {
				angle = angle / 180 * M_PI;
				a = cos(angle);
				b = sin(angle);
				c = -sin(angle);
				d = cos(angle);
	                } else if (sscanf(s, "skewX (%f) %n",
					&angle, &n) == 1) {
				angle = angle / 180 * M_PI;
				c = tan(angle);
	                } else if (sscanf(s, "skewY (%f) %n",
					&angle, &n) == 1) {
				angle = angle / 180 * M_PI;
				b = tan(angle);
	                } else
				break;
			ctm_a = state->ctm.a * a + state->ctm.c * b;
			ctm_b = state->ctm.b * a + state->ctm.d * b;
			ctm_c = state->ctm.a * c + state->ctm.c * d;
			ctm_d = state->ctm.b * c + state->ctm.d * d;
			ctm_e = state->ctm.a * e + state->ctm.c * f +
					state->ctm.e;
			ctm_f = state->ctm.b * e + state->ctm.d * f +
					state->ctm.f;
			state->ctm.a = ctm_a;
			state->ctm.b = ctm_b;
			state->ctm.c = ctm_c;
			state->ctm.d = ctm_d;
			state->ctm.e = ctm_e;
			state->ctm.f = ctm_f;
			s += n;
		}

		xmlFree(transform);
	}
}


/**
 * Add a svgtiny_shape to the svgtiny_diagram.
 */

struct svgtiny_shape *svgtiny_add_shape(struct svgtiny_parse_state *state)
{
	struct svgtiny_shape *shape = realloc(state->diagram->shape,
			(state->diagram->shape_count + 1) *
			sizeof (state->diagram->shape[0]));
	if (!shape)
		return 0;
	state->diagram->shape = shape;

	shape += state->diagram->shape_count;
	shape->path = 0;
	shape->path_length = 0;
	shape->text = 0;
	shape->fill = state->fill;
	shape->stroke = state->stroke;
	shape->stroke_width = state->stroke_width *
			(state->ctm.a + state->ctm.d) / 2;

	return shape;
}


void svgtiny_transform_path(float *p, unsigned int n,
		struct svgtiny_parse_state *state)
{
	for (unsigned int j = 0; j != n; ) {
		unsigned int points = 0;
		switch ((int) p[j]) {
		case svgtiny_PATH_MOVE:
		case svgtiny_PATH_LINE:
			points = 1;
			break;
		case svgtiny_PATH_CLOSE:
			points = 0;
			break;
		case svgtiny_PATH_BEZIER:
			points = 3;
			break;
		default:
			assert(0);
		}
		j++;
		for (unsigned int k = 0; k != points; k++) {
			float x0 = p[j], y0 = p[j + 1];
			float x = state->ctm.a * x0 + state->ctm.c * y0 +
				state->ctm.e;
			float y = state->ctm.b * x0 + state->ctm.d * y0 +
				state->ctm.f;
			p[j] = x;
			p[j + 1] = y;
			j += 2;
		}
	}
}


void svgtiny_free(struct svgtiny_diagram *svg)
{
	assert(svg);

	for (unsigned int i = 0; i != svg->shape_count; i++) {
		free(svg->shape[i].path);
		free(svg->shape[i].text);
	}
	
	free(svg->shape);

	free(svg);
}

