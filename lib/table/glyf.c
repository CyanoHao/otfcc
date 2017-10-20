#include "glyf.h"

#include "support/util.h"
#include "support/ttinstr/ttinstr.h"

// point
static void createPoint(glyf_Point *p) {
	p->x = iVQ.createStill(0);
	p->y = iVQ.createStill(0);
	p->onCurve = true;
}
static void copyPoint(glyf_Point *dst, const glyf_Point *src) {
	iVQ.copy(&dst->x, &src->x);
	iVQ.copy(&dst->y, &src->y);
	dst->onCurve = src->onCurve;
}
static void disposePoint(glyf_Point *p) {
	iVQ.dispose(&p->x);
	iVQ.dispose(&p->y);
}

caryll_standardValType(glyf_Point, glyf_iPoint, createPoint, copyPoint, disposePoint);

// contour
caryll_standardVectorImpl(glyf_Contour, glyf_Point, glyf_iPoint, glyf_iContour);
caryll_standardVectorImpl(glyf_ContourList, glyf_Contour, glyf_iContour, glyf_iContourList);

// ref
static INLINE void initGlyfReference(glyf_ComponentReference *ref) {
	ref->glyph = Handle.empty();
	ref->x = iVQ.createStill(0);
	ref->y = iVQ.createStill(0);
	ref->a = 1;
	ref->b = 0;
	ref->c = 0;
	ref->d = 1;
	ref->isAnchored = REF_XY;
	ref->inner = ref->outer = 0;
	ref->roundToGrid = false;
	ref->useMyMetrics = false;
}
static void copyGlyfReference(glyf_ComponentReference *dst, const glyf_ComponentReference *src) {
	iVQ.copy(&dst->x, &src->x);
	iVQ.copy(&dst->y, &src->y);
	Handle.copy(&dst->glyph, &src->glyph);
	dst->a = src->a;
	dst->b = src->b;
	dst->c = src->c;
	dst->d = src->d;
	dst->isAnchored = src->isAnchored;
	dst->inner = src->inner;
	dst->outer = src->outer;
	dst->roundToGrid = src->roundToGrid;
	dst->useMyMetrics = src->useMyMetrics;
}
static INLINE void disposeGlyfReference(glyf_ComponentReference *ref) {
	iVQ.dispose(&ref->x);
	iVQ.dispose(&ref->y);
	Handle.dispose(&ref->glyph);
}
caryll_standardValType(glyf_ComponentReference, glyf_iComponentReference, initGlyfReference,
                       copyGlyfReference, disposeGlyfReference);
caryll_standardVectorImpl(glyf_ReferenceList, glyf_ComponentReference, glyf_iComponentReference,
                          glyf_iReferenceList);

// stem
caryll_standardType(glyf_PostscriptStemDef, glyf_iPostscriptStemDef);
caryll_standardVectorImpl(glyf_StemDefList, glyf_PostscriptStemDef, glyf_iPostscriptStemDef,
                          glyf_iStemDefList);

// mask
caryll_standardType(glyf_PostscriptHintMask, glyf_iPostscriptHintMask);
caryll_standardVectorImpl(glyf_MaskList, glyf_PostscriptHintMask, glyf_iPostscriptHintMask,
                          glyf_iMaskList);

typedef enum {
	GLYF_FLAG_ON_CURVE = 1,
	GLYF_FLAG_X_SHORT = (1 << 1),
	GLYF_FLAG_Y_SHORT = (1 << 2),
	GLYF_FLAG_REPEAT = (1 << 3),
	GLYF_FLAG_SAME_X = (1 << 4),
	GLYF_FLAG_SAME_Y = (1 << 5),
	GLYF_FLAG_POSITIVE_X = (1 << 4),
	GLYF_FLAG_POSITIVE_Y = (1 << 5)
} glyf_point_flag;

typedef enum {
	ARG_1_AND_2_ARE_WORDS = (1 << 0),
	ARGS_ARE_XY_VALUES = (1 << 1),
	ROUND_XY_TO_GRID = (1 << 2),
	WE_HAVE_A_SCALE = (1 << 3),
	MORE_COMPONENTS = (1 << 5),
	WE_HAVE_AN_X_AND_Y_SCALE = (1 << 6),
	WE_HAVE_A_TWO_BY_TWO = (1 << 7),
	WE_HAVE_INSTRUCTIONS = (1 << 8),
	USE_MY_METRICS = (1 << 9),
	OVERLAP_COMPOUND = (1 << 10)
} glyf_reference_flag;

typedef enum { MASK_ON_CURVE = 1 } glyf_oncurve_mask;

glyf_Glyph *otfcc_newGlyf_glyph() {
	glyf_Glyph *g;
	NEW(g);
	g->name = NULL;
	g->horizontalOrigin = 0;
	g->advanceWidth = 0;
	g->verticalOrigin = 0;
	g->advanceHeight = 0;

	glyf_iContourList.init(&g->contours);
	glyf_iReferenceList.init(&g->references);
	glyf_iStemDefList.init(&g->stemH);
	glyf_iStemDefList.init(&g->stemV);
	glyf_iMaskList.init(&g->hintMasks);
	glyf_iMaskList.init(&g->contourMasks);

	g->instructionsLength = 0;
	g->instructions = NULL;
	g->fdSelect = Handle.empty();
	g->yPel = 0;

	g->stat.xMin = 0;
	g->stat.xMax = 0;
	g->stat.yMin = 0;
	g->stat.yMax = 0;
	g->stat.nestDepth = 0;
	g->stat.nPoints = 0;
	g->stat.nContours = 0;
	g->stat.nCompositePoints = 0;
	g->stat.nCompositeContours = 0;
	return g;
}
static void otfcc_deleteGlyf_glyph(glyf_Glyph *g) {
	if (!g) return;
	sdsfree(g->name);
	glyf_iContourList.dispose(&g->contours);
	glyf_iReferenceList.dispose(&g->references);
	glyf_iStemDefList.dispose(&g->stemH);
	glyf_iStemDefList.dispose(&g->stemV);
	glyf_iMaskList.dispose(&g->hintMasks);
	glyf_iMaskList.dispose(&g->contourMasks);
	if (g->instructions) { FREE(g->instructions); }
	Handle.dispose(&g->fdSelect);
	g->name = NULL;
	FREE(g);
}

static INLINE void initGlyfPtr(glyf_GlyphPtr *g) {
	*g = NULL;
}
static void copyGlyfPtr(glyf_GlyphPtr *dst, const glyf_GlyphPtr *src) {
	*dst = *src;
}
static INLINE void disposeGlyfPtr(glyf_GlyphPtr *g) {
	otfcc_deleteGlyf_glyph(*g);
}
caryll_ElementInterfaceOf(glyf_GlyphPtr) glyf_iGlyphPtr = {
    .init = initGlyfPtr, .copy = copyGlyfPtr, .dispose = disposeGlyfPtr,
};
caryll_standardVectorImpl(table_glyf, glyf_GlyphPtr, glyf_iGlyphPtr, table_iGlyf);

static glyf_Point *next_point(glyf_ContourList *contours, shapeid_t *cc, shapeid_t *cp) {
	if (*cp >= contours->items[*cc].length) {
		*cp = 0;
		*cc += 1;
	}
	return &contours->items[*cc].items[(*cp)++];
}

static glyf_Glyph *otfcc_read_simple_glyph(font_file_pointer start, shapeid_t numberOfContours,
                                           const otfcc_Options *options) {
	glyf_Glyph *g = otfcc_newGlyf_glyph();
	glyf_ContourList *contours = &g->contours;

	shapeid_t pointsInGlyph = 0;
	for (shapeid_t j = 0; j < numberOfContours; j++) {
		shapeid_t lastPointInCurrentContour = read_16u(start + 2 * j);
		glyf_Contour contour;
		glyf_iContour.init(&contour);
		glyf_iContour.fill(&contour, lastPointInCurrentContour - pointsInGlyph + 1);
		glyf_iContourList.push(contours, contour);
		pointsInGlyph = lastPointInCurrentContour + 1;
	}
	uint16_t instructionLength = read_16u(start + 2 * numberOfContours);
	uint8_t *instructions = NULL;
	if (instructionLength > 0) {
		NEW(instructions, instructionLength);
		memcpy(instructions, start + 2 * numberOfContours + 2, sizeof(uint8_t) * instructionLength);
	}
	g->instructionsLength = instructionLength;
	g->instructions = instructions;

	// read flags
	// There are repeating entries in the flags list, we will fill out the
	// result
	font_file_pointer flags;
	NEW(flags, pointsInGlyph);
	font_file_pointer flagStart = start + 2 * numberOfContours + 2 + instructionLength;
	shapeid_t flagsReadSofar = 0;
	shapeid_t flagBytesReadSofar = 0;

	shapeid_t currentContour = 0;
	shapeid_t currentContourPointIndex = 0;
	while (flagsReadSofar < pointsInGlyph) {
		uint8_t flag = flagStart[flagBytesReadSofar];
		flags[flagsReadSofar] = flag;
		flagBytesReadSofar += 1;
		flagsReadSofar += 1;
		next_point(contours, &currentContour, &currentContourPointIndex)->onCurve =
		    (flag & GLYF_FLAG_ON_CURVE);
		if (flag & GLYF_FLAG_REPEAT) { // repeating flag
			uint8_t repeat = flagStart[flagBytesReadSofar];
			flagBytesReadSofar += 1;
			for (uint8_t j = 0; j < repeat; j++) {
				flags[flagsReadSofar + j] = flag;
				next_point(contours, &currentContour, &currentContourPointIndex)->onCurve =
				    (flag & GLYF_FLAG_ON_CURVE);
			}
			flagsReadSofar += repeat;
		}
	}

	// read X coordinates
	font_file_pointer coordinatesStart = flagStart + flagBytesReadSofar;
	uint32_t coordinatesOffset = 0;
	shapeid_t coordinatesRead = 0;
	currentContour = 0;
	currentContourPointIndex = 0;
	while (coordinatesRead < pointsInGlyph) {
		uint8_t flag = flags[coordinatesRead];
		int16_t x;
		if (flag & GLYF_FLAG_X_SHORT) {
			x = (flag & GLYF_FLAG_POSITIVE_X ? 1 : -1) *
			    read_8u(coordinatesStart + coordinatesOffset);
			coordinatesOffset += 1;
		} else {
			if (flag & GLYF_FLAG_SAME_X) {
				x = 0;
			} else {
				x = read_16s(coordinatesStart + coordinatesOffset);
				coordinatesOffset += 2;
			}
		}
		iVQ.replace(&(next_point(contours, &currentContour, &currentContourPointIndex)->x),
		            iVQ.createStill(x));
		coordinatesRead += 1;
	}
	// read Y, identical to X
	coordinatesRead = 0;
	currentContour = 0;
	currentContourPointIndex = 0;
	while (coordinatesRead < pointsInGlyph) {
		uint8_t flag = flags[coordinatesRead];
		int16_t y;
		if (flag & GLYF_FLAG_Y_SHORT) {
			y = (flag & GLYF_FLAG_POSITIVE_Y ? 1 : -1) *
			    read_8u(coordinatesStart + coordinatesOffset);
			coordinatesOffset += 1;
		} else {
			if (flag & GLYF_FLAG_SAME_Y) {
				y = 0;
			} else {
				y = read_16s(coordinatesStart + coordinatesOffset);
				coordinatesOffset += 2;
			}
		}
		iVQ.replace(&(next_point(contours, &currentContour, &currentContourPointIndex)->y),
		            iVQ.createStill(y));
		coordinatesRead += 1;
	}
	FREE(flags);
	// turn deltas to absolute coordiantes
	double cx = 0;
	double cy = 0;
	for (shapeid_t j = 0; j < numberOfContours; j++) {
		for (shapeid_t k = 0; k < contours->items[j].length; k++) {
			glyf_Point *z = &contours->items[j].items[k];
			cx += iVQ.getStill(z->x);
			iVQ.dispose(&z->x);
			z->x = iVQ.createStill(cx);

			cy += iVQ.getStill(z->y);
			iVQ.dispose(&z->y);
			z->y = iVQ.createStill(cy);
		}
	}
	return g;
}

static glyf_Glyph *otfcc_read_composite_glyph(font_file_pointer start,
                                              const otfcc_Options *options) {
	glyf_Glyph *g = otfcc_newGlyf_glyph();

	// pass 1, read references quantity
	uint16_t flags = 0;
	uint32_t offset = 0;
	bool glyphHasInstruction = false;
	do {
		flags = read_16u(start + offset);
		glyphid_t index = read_16u(start + offset + 2);

		glyf_ComponentReference ref = glyf_iComponentReference.empty();
		ref.glyph = Handle.fromIndex(index);

		offset += 4; // flags & index
		if (flags & ARGS_ARE_XY_VALUES) {
			ref.isAnchored = REF_XY;
			if (flags & ARG_1_AND_2_ARE_WORDS) {
				ref.x = iVQ.createStill(read_16s(start + offset));
				ref.y = iVQ.createStill(read_16s(start + offset + 2));
				offset += 4;
			} else {
				ref.x = iVQ.createStill(read_8s(start + offset));
				ref.y = iVQ.createStill(read_8s(start + offset + 1));
				offset += 2;
			}
		} else {
			ref.isAnchored = REF_ANCHOR_ANCHOR;
			if (flags & ARG_1_AND_2_ARE_WORDS) {
				ref.outer = read_16u(start + offset);
				ref.inner = read_16u(start + offset + 2);
				offset += 4;
			} else {
				ref.outer = read_8u(start + offset);
				ref.inner = read_8u(start + offset + 1);
				offset += 2;
			}
		}
		if (flags & WE_HAVE_A_SCALE) {
			ref.a = ref.d = otfcc_from_f2dot14(read_16s(start + offset));
			offset += 2;
		} else if (flags & WE_HAVE_AN_X_AND_Y_SCALE) {
			ref.a = otfcc_from_f2dot14(read_16s(start + offset));
			ref.d = otfcc_from_f2dot14(read_16s(start + offset + 2));
			offset += 4;
		} else if (flags & WE_HAVE_A_TWO_BY_TWO) {
			ref.a = otfcc_from_f2dot14(read_16s(start + offset));
			ref.b = otfcc_from_f2dot14(read_16s(start + offset + 2));
			ref.c = otfcc_from_f2dot14(read_16s(start + offset + 4));
			ref.d = otfcc_from_f2dot14(read_16s(start + offset + 2));
			offset += 8;
		}
		ref.roundToGrid = flags & ROUND_XY_TO_GRID;
		ref.useMyMetrics = flags & USE_MY_METRICS;
		if (flags & WE_HAVE_INSTRUCTIONS) { glyphHasInstruction = true; }
		glyf_iReferenceList.push(&g->references, ref);
	} while (flags & MORE_COMPONENTS);

	if (glyphHasInstruction) {
		uint16_t instructionLength = read_16u(start + offset);
		font_file_pointer instructions = NULL;
		if (instructionLength > 0) {
			NEW(instructions, instructionLength);
			memcpy(instructions, start + offset + 2, sizeof(uint8_t) * instructionLength);
		}
		g->instructionsLength = instructionLength;
		g->instructions = instructions;
	} else {
		g->instructionsLength = 0;
		g->instructions = NULL;
	}

	return g;
}

static glyf_Glyph *otfcc_read_glyph(font_file_pointer data, uint32_t offset,
                                    const otfcc_Options *options) {
	font_file_pointer start = data + offset;
	int16_t numberOfContours = read_16u(start);
	glyf_Glyph *g;
	if (numberOfContours > 0) {
		g = otfcc_read_simple_glyph(start + 10, numberOfContours, options);
	} else {
		g = otfcc_read_composite_glyph(start + 10, options);
	}
	g->stat.xMin = read_16s(start + 2);
	g->stat.yMin = read_16s(start + 4);
	g->stat.xMax = read_16s(start + 6);
	g->stat.yMax = read_16s(start + 8);
	return g;
}

table_glyf *otfcc_readGlyf(const otfcc_Packet packet, const otfcc_Options *options,
                           table_head *head, table_maxp *maxp) {
	if (head == NULL || maxp == NULL) return NULL;
	uint32_t *offsets = NULL;
	table_glyf *glyf = NULL;

	uint16_t locaIsLong = head->indexToLocFormat;
	glyphid_t numGlyphs = maxp->numGlyphs;
	NEW(offsets, (numGlyphs + 1));
	if (!offsets) goto ABSENT;
	bool foundLoca = false;

	// read loca
	FOR_TABLE('loca', table) {
		font_file_pointer data = table.data;
		uint32_t length = table.length;
		if (length < 2 * numGlyphs + 2) goto LOCA_CORRUPTED;
		for (uint32_t j = 0; j < numGlyphs + 1; j++) {
			if (locaIsLong) {
				offsets[j] = read_32u(data + j * 4);
			} else {
				offsets[j] = read_16u(data + j * 2) * 2;
			}
			if (j > 0 && offsets[j] < offsets[j - 1]) goto LOCA_CORRUPTED;
		}
		foundLoca = true;
		break;
	LOCA_CORRUPTED:
		logWarning("table 'loca' corrupted.\n");
		if (offsets) { FREE(offsets), offsets = NULL; }
		continue;
	}
	if (!foundLoca) goto ABSENT;

	// read glyf
	FOR_TABLE('glyf', table) {
		font_file_pointer data = table.data;
		uint32_t length = table.length;
		if (length < offsets[numGlyphs]) goto GLYF_CORRUPTED;

		glyf = table_iGlyf.create();

		for (glyphid_t j = 0; j < numGlyphs; j++) {
			if (offsets[j] < offsets[j + 1]) { // non-space glyph
				table_iGlyf.push(glyf, otfcc_read_glyph(data, offsets[j], options));
			} else { // space glyph
				table_iGlyf.push(glyf, otfcc_newGlyf_glyph());
			}
		}
		goto PRESENT;
	GLYF_CORRUPTED:
		logWarning("table 'glyf' corrupted.\n");
		if (glyf) { DELETE(table_iGlyf.free, glyf), glyf = NULL; }
	}
	goto ABSENT;

PRESENT:
	if (offsets) { FREE(offsets), offsets = NULL; }
	return glyf;

ABSENT:
	if (offsets) { FREE(offsets), offsets = NULL; }
	if (glyf) { FREE(glyf), glyf = NULL; }
	return NULL;
}

// to json
static void glyf_glyph_dump_contours(glyf_Glyph *g, json_value *target) {
	if (!g->contours.length) return;
	json_value *contours = json_array_new(g->contours.length);
	for (shapeid_t k = 0; k < g->contours.length; k++) {
		glyf_Contour *c = &(g->contours.items[k]);
		json_value *contour = json_array_new(c->length);
		for (shapeid_t m = 0; m < c->length; m++) {
			json_value *point = json_object_new(4);
			json_object_push(point, "x", json_new_VQ(c->items[m].x));
			json_object_push(point, "y", json_new_VQ(c->items[m].y));
			json_object_push(point, "on", json_boolean_new(c->items[m].onCurve & MASK_ON_CURVE));
			json_array_push(contour, point);
		}
		json_array_push(contours, contour);
	}
	json_object_push(target, "contours", preserialize(contours));
}
static void glyf_glyph_dump_references(glyf_Glyph *g, json_value *target) {
	if (!g->references.length) return;
	json_value *references = json_array_new(g->references.length);
	for (shapeid_t k = 0; k < g->references.length; k++) {
		glyf_ComponentReference *r = &(g->references.items[k]);
		json_value *ref = json_object_new(9);
		json_object_push(ref, "glyph",
		                 json_string_new_length((uint32_t)sdslen(r->glyph.name), r->glyph.name));
		json_object_push(ref, "x", json_new_VQ(r->x));
		json_object_push(ref, "y", json_new_VQ(r->y));
		json_object_push(ref, "a", json_new_position(r->a));
		json_object_push(ref, "b", json_new_position(r->b));
		json_object_push(ref, "c", json_new_position(r->c));
		json_object_push(ref, "d", json_new_position(r->d));
		if (r->isAnchored != REF_XY) {
			json_object_push(ref, "isAnchored", json_boolean_new(true));
			json_object_push(ref, "inner", json_integer_new(r->inner));
			json_object_push(ref, "outer", json_integer_new(r->outer));
		}
		if (r->roundToGrid) { json_object_push(ref, "roundToGrid", json_boolean_new(true)); }
		if (r->useMyMetrics) { json_object_push(ref, "useMyMetrics", json_boolean_new(true)); }
		json_array_push(references, ref);
	}
	json_object_push(target, "references", preserialize(references));
}
static json_value *glyf_glyph_dump_stemdefs(glyf_StemDefList *stems) {
	json_value *a = json_array_new(stems->length);
	for (shapeid_t j = 0; j < stems->length; j++) {
		json_value *stem = json_object_new(3);
		json_object_push(stem, "position", json_new_position(stems->items[j].position));
		json_object_push(stem, "width", json_new_position(stems->items[j].width));
		json_array_push(a, stem);
	}
	return a;
}
static json_value *glyf_glyph_dump_maskdefs(glyf_MaskList *masks, glyf_StemDefList *hh,
                                            glyf_StemDefList *vv) {
	json_value *a = json_array_new(masks->length);
	for (shapeid_t j = 0; j < masks->length; j++) {
		json_value *mask = json_object_new(3);
		json_object_push(mask, "contoursBefore", json_integer_new(masks->items[j].contoursBefore));
		json_object_push(mask, "pointsBefore", json_integer_new(masks->items[j].pointsBefore));
		json_value *h = json_array_new(hh->length);
		for (shapeid_t k = 0; k < hh->length; k++) {
			json_array_push(h, json_boolean_new(masks->items[j].maskH[k]));
		}
		json_object_push(mask, "maskH", h);
		json_value *v = json_array_new(vv->length);
		for (shapeid_t k = 0; k < vv->length; k++) {
			json_array_push(v, json_boolean_new(masks->items[j].maskV[k]));
		}
		json_object_push(mask, "maskV", v);
		json_array_push(a, mask);
	}
	return a;
}

static json_value *glyf_dump_glyph(glyf_Glyph *g, const otfcc_Options *options,
                                   bool hasVerticalMetrics, bool exportFDSelect) {
	json_value *glyph = json_object_new(12);
	json_object_push(glyph, "advanceWidth", json_new_position(g->advanceWidth));
	if (fabs(g->horizontalOrigin) > 1.0 / 1000.0) {
		json_object_push(glyph, "horizontalOrigin", json_new_position(g->horizontalOrigin));
	}
	if (hasVerticalMetrics) {
		json_object_push(glyph, "advanceHeight", json_new_position(g->advanceHeight));
		json_object_push(glyph, "verticalOrigin", json_new_position(g->verticalOrigin));
	}
	glyf_glyph_dump_contours(g, glyph);
	glyf_glyph_dump_references(g, glyph);
	if (exportFDSelect) json_object_push(glyph, "CFF_fdSelect", json_string_new(g->fdSelect.name));

	// hinting data
	if (!options->ignore_hints) {
		if (g->instructions && g->instructionsLength) {
			json_object_push(glyph, "instructions",
			                 dump_ttinstr(g->instructions, g->instructionsLength, options));
		}
		if (g->stemH.length) {
			json_object_push(glyph, "stemH", preserialize(glyf_glyph_dump_stemdefs(&g->stemH)));
		}
		if (g->stemV.length) {
			json_object_push(glyph, "stemV", preserialize(glyf_glyph_dump_stemdefs(&g->stemV)));
		}
		if (g->hintMasks.length) {
			json_object_push(glyph, "hintMasks", preserialize(glyf_glyph_dump_maskdefs(
			                                         &g->hintMasks, &g->stemH, &g->stemV)));
		}
		if (g->contourMasks.length) {
			json_object_push(glyph, "contourMasks", preserialize(glyf_glyph_dump_maskdefs(
			                                            &g->contourMasks, &g->stemH, &g->stemV)));
		}
		if (g->yPel) { json_object_push(glyph, "LTSH_yPel", json_integer_new(g->yPel)); }
	}
	return glyph;
}
void otfcc_dump_glyphorder(const table_glyf *table, json_value *root) {
	if (!table) return;
	json_value *order = json_array_new(table->length);
	for (glyphid_t j = 0; j < table->length; j++) {
		json_array_push(order, json_string_new_length((uint32_t)sdslen(table->items[j]->name),
		                                              table->items[j]->name));
	}
	json_object_push(root, "glyph_order", preserialize(order));
}
void otfcc_dumpGlyf(const table_glyf *table, json_value *root, const otfcc_Options *options,
                    bool hasVerticalMetrics, bool exportFDSelect) {
	if (!table) return;
	loggedStep("glyf") {
		json_value *glyf = json_object_new(table->length);
		for (glyphid_t j = 0; j < table->length; j++) {
			glyf_Glyph *g = table->items[j];
			json_object_push(glyf, g->name,
			                 glyf_dump_glyph(g, options, hasVerticalMetrics, exportFDSelect));
		}
		json_object_push(root, "glyf", glyf);
		if (!options->ignore_glyph_order) otfcc_dump_glyphorder(table, root);
	}
}

// from json
static glyf_Point glyf_parse_point(json_value *pointdump) {
	glyf_Point point;
	glyf_iPoint.init(&point);
	if (!pointdump || pointdump->type != json_object) return point;
	for (uint32_t _k = 0; _k < pointdump->u.object.length; _k++) {
		char *ck = pointdump->u.object.values[_k].name;
		json_value *cv = pointdump->u.object.values[_k].value;
		if (strcmp(ck, "x") == 0) {
			iVQ.replace(&point.x, json_vqOf(cv));
		} else if (strcmp(ck, "y") == 0) {
			iVQ.replace(&point.y, json_vqOf(cv));
		} else if (strcmp(ck, "on") == 0) {
			point.onCurve = json_boolof(cv);
		}
	}
	return point;
}

static void glyf_parse_contours(json_value *col, glyf_Glyph *g) {
	if (!col) { return; }
	shapeid_t nContours = col->u.array.length;
	for (shapeid_t j = 0; j < nContours; j++) {
		json_value *contourdump = col->u.array.values[j];
		glyf_Contour contour;
		glyf_iContour.initCapN(
		    &contour,
		    contourdump && contourdump->type == json_array ? contourdump->u.array.length : 1);
		if (contourdump && contourdump->type == json_array) {
			for (shapeid_t k = 0; k < contourdump->u.array.length; k++) {
				glyf_iContour.push(&contour, glyf_parse_point(contourdump->u.array.values[k]));
			}
		}
		glyf_iContourList.push(&g->contours, contour);
	}
}

static glyf_ComponentReference glyf_parse_reference(json_value *refdump) {
	json_value *_gname = json_obj_get_type(refdump, "glyph", json_string);
	glyf_ComponentReference ref = glyf_iComponentReference.empty();
	if (_gname) {
		ref.glyph = Handle.fromName(sdsnewlen(_gname->u.string.ptr, _gname->u.string.length));
		iVQ.replace(&ref.x, json_vqOf(json_obj_get(refdump, "x")));
		iVQ.replace(&ref.y, json_vqOf(json_obj_get(refdump, "y")));
		ref.a = json_obj_getnum_fallback(refdump, "a", 1.0);
		ref.b = json_obj_getnum_fallback(refdump, "b", 0.0);
		ref.c = json_obj_getnum_fallback(refdump, "c", 0.0);
		ref.d = json_obj_getnum_fallback(refdump, "d", 1.0);
		ref.roundToGrid = json_obj_getbool(refdump, "roundToGrid");
		ref.useMyMetrics = json_obj_getbool(refdump, "useMyMetrics");
		if (json_obj_getbool(refdump, "isAnchored")) {
			ref.isAnchored = REF_ANCHOR_XY;
			ref.inner = json_obj_getint(refdump, "inner");
			ref.outer = json_obj_getint(refdump, "outer");
		}
	} else {
		// Invalid glyph references
		ref.glyph.name = NULL;
		iVQ.replace(&ref.x, iVQ.createStill(0));
		iVQ.replace(&ref.y, iVQ.createStill(0));
		ref.a = 1.0;
		ref.b = 0.0;
		ref.c = 0.0;
		ref.d = 1.0;
		ref.roundToGrid = false;
		ref.useMyMetrics = false;
	}
	return ref;
}
static void glyf_parse_references(json_value *col, glyf_Glyph *g) {
	if (!col) { return; }
	for (shapeid_t j = 0; j < col->u.array.length; j++) {
		glyf_iReferenceList.push(&g->references, glyf_parse_reference(col->u.array.values[j]));
	}
}

static void makeInstrsForGlyph(void *_g, uint8_t *instrs, uint32_t len) {
	glyf_Glyph *g = (glyf_Glyph *)_g;
	g->instructionsLength = len;
	g->instructions = instrs;
}
static void wrongInstrsForGlyph(void *_g, char *reason, int pos) {
	glyf_Glyph *g = (glyf_Glyph *)_g;
	fprintf(stderr, "[OTFCC] TrueType instructions parse error : %s, at %d in /%s\n", reason, pos,
	        g->name);
}

static void parse_stems(json_value *sd, glyf_StemDefList *stems) {
	if (!sd) return;
	for (shapeid_t j = 0; j < sd->u.array.length; j++) {
		json_value *s = sd->u.array.values[j];
		if (s->type != json_object) continue;
		glyf_PostscriptStemDef sdef;
		sdef.map = 0;
		sdef.position = json_obj_getnum(s, "position");
		sdef.width = json_obj_getnum(s, "width");
		glyf_iStemDefList.push(stems, sdef);
	}
}
static void parse_maskbits(bool *arr, json_value *bits) {
	if (!bits) {
		for (shapeid_t j = 0; j < 0x100; j++) {
			arr[j] = false;
		}
	} else {
		for (shapeid_t j = 0; j < 0x100 && j < bits->u.array.length; j++) {
			json_value *b = bits->u.array.values[j];
			switch (b->type) {
				case json_boolean:
					arr[j] = b->u.boolean;
					break;
				case json_integer:
					arr[j] = b->u.integer;
					break;
				case json_double:
					arr[j] = b->u.dbl;
					break;
				default:
					arr[j] = false;
			}
		}
	}
}
static void parse_masks(json_value *md, glyf_MaskList *masks) {
	if (!md) return;
	for (shapeid_t j = 0; j < md->u.array.length; j++) {
		json_value *m = md->u.array.values[j];
		if (m->type != json_object) continue;

		glyf_PostscriptHintMask mask;
		mask.pointsBefore = json_obj_getint(m, "pointsBefore");
		mask.contoursBefore = json_obj_getint(m, "contoursBefore");
		parse_maskbits(&(mask.maskH[0]), json_obj_get_type(m, "maskH", json_array));
		parse_maskbits(&(mask.maskV[0]), json_obj_get_type(m, "maskV", json_array));
		glyf_iMaskList.push(masks, mask);
	}
}

static glyf_Glyph *otfcc_glyf_parse_glyph(json_value *glyphdump, otfcc_GlyphOrderEntry *order_entry,
                                          const otfcc_Options *options) {
	glyf_Glyph *g = otfcc_newGlyf_glyph();
	g->name = sdsdup(order_entry->name);
	g->advanceWidth = json_obj_getnum(glyphdump, "advanceWidth");
	g->horizontalOrigin = json_obj_getnum(glyphdump, "horizontalOrigin");
	g->advanceHeight = json_obj_getnum(glyphdump, "advanceHeight");
	g->verticalOrigin = json_obj_getnum(glyphdump, "verticalOrigin");
	glyf_parse_contours(json_obj_get_type(glyphdump, "contours", json_array), g);
	glyf_parse_references(json_obj_get_type(glyphdump, "references", json_array), g);
	if (!options->ignore_hints) {
		parse_ttinstr(json_obj_get(glyphdump, "instructions"), g, makeInstrsForGlyph,
		              wrongInstrsForGlyph);
		parse_stems(json_obj_get_type(glyphdump, "stemH", json_array), &g->stemH);
		parse_stems(json_obj_get_type(glyphdump, "stemV", json_array), &g->stemV);
		parse_masks(json_obj_get_type(glyphdump, "hintMasks", json_array), &g->hintMasks);
		parse_masks(json_obj_get_type(glyphdump, "contourMasks", json_array), &g->contourMasks);
		g->yPel = json_obj_getint(glyphdump, "LTSH_yPel");
	}
	// Glyph data of other tables
	g->fdSelect = Handle.fromName(json_obj_getsds(glyphdump, "CFF_fdSelect"));
	if (!g->yPel) { g->yPel = json_obj_getint(glyphdump, "yPel"); }
	return g;
}

table_glyf *otfcc_parseGlyf(const json_value *root, otfcc_GlyphOrder *glyph_order,
                            const otfcc_Options *options) {
	if (root->type != json_object || !glyph_order) return NULL;
	table_glyf *glyf = NULL;
	json_value *table;
	if ((table = json_obj_get_type(root, "glyf", json_object))) {
		loggedStep("glyf") {
			glyphid_t numGlyphs = table->u.object.length;
			glyf = table_iGlyf.createN(numGlyphs);
			for (glyphid_t j = 0; j < numGlyphs; j++) {
				sds gname = sdsnewlen(table->u.object.values[j].name,
				                      table->u.object.values[j].name_length);
				json_value *glyphdump = table->u.object.values[j].value;
				otfcc_GlyphOrderEntry *order_entry = NULL;
				HASH_FIND(hhName, glyph_order->byName, gname, sdslen(gname), order_entry);
				if (glyphdump->type == json_object && order_entry &&
				    !glyf->items[order_entry->gid]) {
					glyf->items[order_entry->gid] =
					    otfcc_glyf_parse_glyph(glyphdump, order_entry, options);
				}
				json_value_free(glyphdump);
				json_value *v = json_null_new();
				v->parent = table;
				table->u.object.values[j].value = v;
				sdsfree(gname);
			}
		}
		return glyf;
	}
	return NULL;
}

caryll_Buffer *shrinkFlags(caryll_Buffer *flags) {
	if (!buflen(flags)) return (flags);
	caryll_Buffer *shrunk = bufnew();
	bufwrite8(shrunk, flags->data[0]);
	int repeating = 0;
	for (size_t j = 1; j < buflen(flags); j++) {
		if (flags->data[j] == flags->data[j - 1]) {
			if (repeating && repeating < 0xFE) {
				shrunk->data[shrunk->cursor - 1] += 1;
				repeating += 1;
			} else if (repeating == 0) {
				shrunk->data[shrunk->cursor - 1] |= GLYF_FLAG_REPEAT;
				bufwrite8(shrunk, 1);
				repeating += 1;
			} else {
				repeating = 0;
				bufwrite8(shrunk, flags->data[j]);
			}
		} else {
			repeating = 0;
			bufwrite8(shrunk, flags->data[j]);
		}
	}
	buffree(flags);
	return shrunk;
}

// serialize
#define EPSILON (1e-5)
static void glyf_build_simple(const glyf_Glyph *g, caryll_Buffer *gbuf) {
	caryll_Buffer *flags = bufnew();
	caryll_Buffer *xs = bufnew();
	caryll_Buffer *ys = bufnew();

	bufwrite16b(gbuf, g->contours.length);
	bufwrite16b(gbuf, (int16_t)g->stat.xMin);
	bufwrite16b(gbuf, (int16_t)g->stat.yMin);
	bufwrite16b(gbuf, (int16_t)g->stat.xMax);
	bufwrite16b(gbuf, (int16_t)g->stat.yMax);

	// endPtsOfContours[n]
	shapeid_t ptid = 0;
	for (shapeid_t j = 0; j < g->contours.length; j++) {
		ptid += g->contours.items[j].length;
		bufwrite16b(gbuf, ptid - 1);
	}

	// instructions
	bufwrite16b(gbuf, g->instructionsLength);
	if (g->instructions) bufwrite_bytes(gbuf, g->instructionsLength, g->instructions);

	// flags and points
	bufclear(flags);
	bufclear(xs);
	bufclear(ys);
	int32_t cx = 0;
	int32_t cy = 0;
	for (shapeid_t cj = 0; cj < g->contours.length; cj++) {
		for (shapeid_t k = 0; k < g->contours.items[cj].length; k++) {
			glyf_Point *p = &(g->contours.items[cj].items[k]);
			uint8_t flag = (p->onCurve & MASK_ON_CURVE) ? GLYF_FLAG_ON_CURVE : 0;
			int32_t px = round(iVQ.getStill(p->x));
			int32_t py = round(iVQ.getStill(p->y));
			int16_t dx = (int16_t)(px - cx);
			int16_t dy = (int16_t)(py - cy);
			if (dx == 0) {
				flag |= GLYF_FLAG_SAME_X;
			} else if (dx >= -0xFF && dx <= 0xFF) {
				flag |= GLYF_FLAG_X_SHORT;
				if (dx > 0) {
					flag |= GLYF_FLAG_POSITIVE_X;
					bufwrite8(xs, dx);
				} else {
					bufwrite8(xs, -dx);
				}
			} else {
				bufwrite16b(xs, dx);
			}

			if (dy == 0) {
				flag |= GLYF_FLAG_SAME_Y;
			} else if (dy >= -0xFF && dy <= 0xFF) {
				flag |= GLYF_FLAG_Y_SHORT;
				if (dy > 0) {
					flag |= GLYF_FLAG_POSITIVE_Y;
					bufwrite8(ys, dy);
				} else {
					bufwrite8(ys, -dy);
				}
			} else {
				bufwrite16b(ys, dy);
			}
			bufwrite8(flags, flag);
			cx = px;
			cy = py;
		}
	}
	flags = shrinkFlags(flags);
	bufwrite_buf(gbuf, flags);
	bufwrite_buf(gbuf, xs);
	bufwrite_buf(gbuf, ys);

	buffree(flags);
	buffree(xs);
	buffree(ys);
}
static void glyf_build_composite(const glyf_Glyph *g, caryll_Buffer *gbuf) {
	bufwrite16b(gbuf, (-1));
	bufwrite16b(gbuf, (int16_t)g->stat.xMin);
	bufwrite16b(gbuf, (int16_t)g->stat.yMin);
	bufwrite16b(gbuf, (int16_t)g->stat.xMax);
	bufwrite16b(gbuf, (int16_t)g->stat.yMax);
	for (shapeid_t rj = 0; rj < g->references.length; rj++) {
		glyf_ComponentReference *r = &(g->references.items[rj]);
		uint16_t flags =
		    (rj < g->references.length - 1 ? MORE_COMPONENTS
		                                   : g->instructionsLength > 0 ? WE_HAVE_INSTRUCTIONS : 0);
		bool outputAnchor = r->isAnchored == REF_ANCHOR_CONSOLIDATED;

		union {
			uint16_t pointid;
			int16_t coord;
		} arg1, arg2;

		// flags
		if (outputAnchor) {
			arg1.pointid = r->outer;
			arg2.pointid = r->inner;
			if (!(arg1.pointid < 0x100 && arg2.pointid < 0x100)) { flags |= ARG_1_AND_2_ARE_WORDS; }
		} else {
			flags |= ARGS_ARE_XY_VALUES;
			arg1.coord = iVQ.getStill(r->x);
			arg2.coord = iVQ.getStill(r->y);
			if (!(arg1.coord < 128 && arg1.coord >= -128 && arg2.coord < 128 &&
			      arg2.coord >= -128)) {
				flags |= ARG_1_AND_2_ARE_WORDS;
			}
		}
		if (fabs(r->b) > EPSILON || fabs(r->c) > EPSILON) {
			flags |= WE_HAVE_A_TWO_BY_TWO;
		} else if (fabs(r->a - 1) > EPSILON || fabs(r->d - 1) > EPSILON) {
			if (fabs(r->a - r->d) > EPSILON) {
				flags |= WE_HAVE_AN_X_AND_Y_SCALE;
			} else {
				flags |= WE_HAVE_A_SCALE;
			}
		}
		if (r->roundToGrid) flags |= ROUND_XY_TO_GRID;
		if (r->useMyMetrics) flags |= USE_MY_METRICS;
		bufwrite16b(gbuf, flags);
		bufwrite16b(gbuf, r->glyph.index);
		if (flags & ARG_1_AND_2_ARE_WORDS) {
			bufwrite16b(gbuf, arg1.pointid);
			bufwrite16b(gbuf, arg2.pointid);
		} else {
			bufwrite8(gbuf, arg1.pointid);
			bufwrite8(gbuf, arg2.pointid);
		}
		if (flags & WE_HAVE_A_SCALE) {
			bufwrite16b(gbuf, otfcc_to_f2dot14(r->a));
		} else if (flags & WE_HAVE_AN_X_AND_Y_SCALE) {
			bufwrite16b(gbuf, otfcc_to_f2dot14(r->a));
			bufwrite16b(gbuf, otfcc_to_f2dot14(r->d));
		} else if (flags & WE_HAVE_A_TWO_BY_TWO) {
			bufwrite16b(gbuf, otfcc_to_f2dot14(r->a));
			bufwrite16b(gbuf, otfcc_to_f2dot14(r->b));
			bufwrite16b(gbuf, otfcc_to_f2dot14(r->c));
			bufwrite16b(gbuf, otfcc_to_f2dot14(r->d));
		}
	}
	if (g->instructionsLength) {
		bufwrite16b(gbuf, g->instructionsLength);
		if (g->instructions) bufwrite_bytes(gbuf, g->instructionsLength, g->instructions);
	}
}
table_GlyfAndLocaBuffers otfcc_buildGlyf(const table_glyf *table, table_head *head,
                                         const otfcc_Options *options) {
	caryll_Buffer *bufglyf = bufnew();
	caryll_Buffer *bufloca = bufnew();
	if (table && head) {
		caryll_Buffer *gbuf = bufnew();
		uint32_t *loca;
		NEW(loca, table->length + 1);
		for (glyphid_t j = 0; j < table->length; j++) {
			loca[j] = (uint32_t)bufglyf->cursor;
			glyf_Glyph *g = table->items[j];
			bufclear(gbuf);
			if (g->contours.length > 0) {
				glyf_build_simple(g, gbuf);
			} else if (g->references.length > 0) {
				glyf_build_composite(g, gbuf);
			}
			// pad extra zeroes
			buflongalign(gbuf);
			bufwrite_buf(bufglyf, gbuf);
		}
		loca[table->length] = (uint32_t)bufglyf->cursor;
		if (bufglyf->cursor >= 0x20000) {
			head->indexToLocFormat = 1;
		} else {
			head->indexToLocFormat = 0;
		}
		// write loca table
		for (uint32_t j = 0; j <= table->length; j++) {
			if (head->indexToLocFormat) {
				bufwrite32b(bufloca, loca[j]);
			} else {
				bufwrite16b(bufloca, loca[j] >> 1);
			}
		}
		buffree(gbuf);
		FREE(loca);
	}
	table_GlyfAndLocaBuffers pair = {bufglyf, bufloca};
	return pair;
}
