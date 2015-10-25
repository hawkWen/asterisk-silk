#include "asterisk.h"

#include "math.h"

#include "asterisk/module.h"
#include "asterisk/format.h"

#include <SKP_Silk_SDK_API.h>

/* Asterisk internal defaults; can differ from RFC defaults */ 
static SKP_SILK_SDK_EncControlStruct default_silk_attr = {
	.useDTX               = 0,             /* off              */
	.useInBandFEC         = 1,             /* supported        */
	.packetLossPercentage = 0,             /* no loss expected */
	.bitRate              = SKP_int32_MAX, /* give everything  */
};

static void silk_destroy(struct ast_format *format)
{
	SKP_SILK_SDK_EncControlStruct *attr = ast_format_get_attribute_data(format);

	ast_free(attr);
}

static int silk_clone(const struct ast_format *src, struct ast_format *dst)
{
	SKP_SILK_SDK_EncControlStruct *original = ast_format_get_attribute_data(src);
	SKP_SILK_SDK_EncControlStruct *attr = ast_calloc(1, sizeof(*attr));

	if (!attr) {
		return -1;
	}

	if (original) {
		*attr = *original;
	}

	ast_format_set_attribute_data(dst, attr);

	return 0;
}

static struct ast_format *silk_parse_sdp_fmtp(const struct ast_format *format, const char *attributes)
{
	struct ast_format *cloned;
	SKP_SILK_SDK_EncControlStruct *attr;
	unsigned int val;

	cloned = ast_format_clone(format);
	if (!cloned) {
		return NULL;
	}
	attr = ast_format_get_attribute_data(cloned);

	attr->bitRate = SKP_int32_MAX;
	if (sscanf(attributes, "maxaveragebitrate=%30u", &val) == 1) {
		attr->bitRate = val;
	}
	attr->useDTX = 0;
	if (sscanf(attributes, "usedtx=%30u", &val) == 1) {
		attr->useDTX = val;
	}
	attr->useInBandFEC = 1;
	if (sscanf(attributes, "useinbandfec=%30u", &val) == 1) {
		attr->useInBandFEC = val;
	}

	return cloned;
}

static void silk_generate_sdp_fmtp(const struct ast_format *format, unsigned int payload, struct ast_str **str)
{
	SKP_SILK_SDK_EncControlStruct *attr = ast_format_get_attribute_data(format);

	if (!attr) {
		attr = &default_silk_attr;
	}

	if (attr->bitRate != SKP_int32_MAX) { 
		ast_str_append(str, 0, "a=fmtp:%u maxaveragebitrate=%u\r\n", payload, attr->bitRate);
	}

	if (attr->useDTX != 0) { 
		ast_str_append(str, 0, "a=fmtp:%u usedtx=%u\r\n", payload, attr->useDTX);
	}

	if (attr->useInBandFEC != 1) { 
		ast_str_append(str, 0, "a=fmtp:%u useinbandfec=%u\r\n", payload, attr->useInBandFEC);
	}
}

static enum ast_format_cmp_res silk_cmp(const struct ast_format *format1, const struct ast_format *format2)
{
	SKP_SILK_SDK_EncControlStruct *attr1 = ast_format_get_attribute_data(format1);
	SKP_SILK_SDK_EncControlStruct *attr2 = ast_format_get_attribute_data(format2);

	if (attr1 && attr1->bitRate < 5000) {
		return AST_FORMAT_CMP_NOT_EQUAL;
	}

	if (attr2 && attr2->bitRate < 5000) {
		return AST_FORMAT_CMP_NOT_EQUAL;
	}

	return AST_FORMAT_CMP_EQUAL;
}

static struct ast_format *silk_getjoint(const struct ast_format *format1, const struct ast_format *format2)
{
	struct ast_format *jointformat;
	SKP_SILK_SDK_EncControlStruct *attr1 = ast_format_get_attribute_data(format1);
	SKP_SILK_SDK_EncControlStruct *attr2 = ast_format_get_attribute_data(format2);
	SKP_SILK_SDK_EncControlStruct *attr_res;

	if (!attr1) {
		attr1 = &default_silk_attr;
	}

	if (!attr2) {
		attr2 = &default_silk_attr;
	}

	jointformat = ast_format_clone(format1);
	if (!jointformat) {
		return NULL;
	}
	attr_res = ast_format_get_attribute_data(jointformat);

	/* Take the lowest max bitrate */
	attr_res->bitRate = MIN(attr1->bitRate, attr2->bitRate);

	/* Do DTX if one sides want it. */
	attr_res->useDTX = attr1->useDTX || attr2->useDTX;

	/* Only do FEC if both sides support it. If a peer does not
	 * support FEC, we would waste bandwidth. */
	attr_res->useInBandFEC = attr1->useInBandFEC && attr2->useInBandFEC ? 1 : 0;

	/* Use the maximum packetloss percentage between the two attributes. This affects how
	 * much redundancy is used in the FEC. */
	attr_res->packetLossPercentage = MAX(attr1->packetLossPercentage, attr2->packetLossPercentage);

	return jointformat;
}

static struct ast_format_interface silk_interface = {
	.format_destroy = silk_destroy,
	.format_clone = silk_clone,
	.format_cmp = silk_cmp,
	.format_get_joint = silk_getjoint,
	.format_attribute_set = NULL,
	.format_parse_sdp_fmtp = silk_parse_sdp_fmtp,
	.format_generate_sdp_fmtp = silk_generate_sdp_fmtp,
};

static int load_module(void)
{
	if (ast_format_interface_register("silk8", &silk_interface)) {
		return AST_MODULE_LOAD_DECLINE;
	}
	ast_format_interface_register("silk12", &silk_interface);
	ast_format_interface_register("silk16", &silk_interface);
	ast_format_interface_register("silk24", &silk_interface);

	return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void)
{
	return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER,
	"SILK Format Attribute Module",
	.support_level = AST_MODULE_SUPPORT_CORE,
	.load = load_module,
	.unload = unload_module,
	.load_pri = AST_MODPRI_CHANNEL_DEPEND,
);