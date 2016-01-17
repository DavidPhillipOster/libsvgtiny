/*
 * This file is an optional part of Libsvgtiny
 * Licensed under the MIT License,
 *                http://opensource.org/licenses/mit-license.php
 *
 * It allows you to write errors to stderr.
 *
 * Copyright 2016 by David Phillip Oster
 */

#ifndef svgtiny_report_err_h
#define svgtiny_report_err_h

#include <stdio.h>

#include "svgtiny.h"

void svgtiny_report_err(svgtiny_code code, struct svgtiny_diagram *diagram);

#endif /* svgtiny_report_err_h */
