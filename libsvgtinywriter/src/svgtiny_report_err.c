/*
 * This file is an optional part of Libsvgtiny
 * Licensed under the MIT License,
 *                http://opensource.org/licenses/mit-license.php
 *
 * It allows you to write errors to stderr.
 *
 * Copyright 2016 by David Phillip Oster
 */

#include "svgtiny_report_err.h"

#include <stdio.h>

void svgtiny_report_err(svgtiny_code code, struct svgtiny_diagram *diagram) {
	if (code != svgtiny_OK) {
		fprintf(stderr, "svgtiny_parse failed: ");
		switch (code) {
		case svgtiny_OUT_OF_MEMORY:
			fprintf(stderr, "svgtiny_OUT_OF_MEMORY");
			break;
		case svgtiny_LIBDOM_ERROR:
			fprintf(stderr, "svgtiny_LIBDOM_ERROR");
			break;
		case svgtiny_NOT_SVG:
			fprintf(stderr, "svgtiny_NOT_SVG");
			break;
		case svgtiny_SVG_ERROR:
			fprintf(stderr, "svgtiny_SVG_ERROR: line %i: %s",
					diagram->error_line,
					diagram->error_message);
			break;
		default:
			fprintf(stderr, "unknown svgtiny_code %i", code);
			break;
		}
		fprintf(stderr, "\n");
	}
}