import os
import re

ioctls = [
        ('DRM_IOCTL_SET_VERSION', 'modeset', [
            ('struct drm_set_version set_version', 'inarg')]),
        ('DRM_IOCTL_GET_UNIQUE', 'modeset', [
            ('struct drm_unique uniq', 'inarg'),
            ('char uniq_str[4096]', 'uniq.unique', 'uniq.unique_len')]),
        ('DRM_IOCTL_VERSION', 'modeset', [
            ('struct drm_version version', 'inarg'),
            ('char name[4096]', 'version.name', 'version.name_len'),
            ('char date[4096]', 'version.date', 'version.date_len'),
            ('char desc[4096]', 'version.desc', 'version.desc_len')]),
        ('DRM_IOCTL_MODE_GETRESOURCES', 'modeset', [
            ('struct drm_mode_card_res res', 'inarg'),
            ('uint32_t fbs[1024]', '(uint64_t)res.fb_id_ptr', 'res.count_fbs * 4'),
            ('uint32_t crtcs[1024]', '(uint64_t)res.crtc_id_ptr', 'res.count_crtcs * 4'),
            ('uint32_t connectors[1024]', '(uint64_t)res.connector_id_ptr', 'res.count_connectors * 4'),
            ('uint32_t encoders[1024]', '(uint64_t)res.encoder_id_ptr', 'res.count_encoders * 4')]),
        ('DRM_IOCTL_MODE_GETCONNECTOR', 'modeset', [
            ('struct drm_mode_get_connector get_connector', 'inarg'),
            ('uint32_t encoders[1024]', '(uint64_t)get_connector.encoders_ptr', 'get_connector.count_encoders * 4'),
            ('struct drm_mode_modeinfo modes[1024]', '(uint64_t)get_connector.modes_ptr', 'get_connector.count_modes * sizeof(struct drm_mode_modeinfo)'),
            ('uint32_t props[1024]', '(uint64_t)get_connector.props_ptr', 'get_connector.count_props * 4'),
            ('uint64_t prop_values[1024]', '(uint64_t)get_connector.prop_values_ptr', 'get_connector.count_props * 8')]),
        ('DRM_IOCTL_MODE_GETENCODER', 'modeset', [
            ('struct drm_mode_get_encoder encoder', 'inarg')]),
        ('DRM_IOCTL_RADEON_INFO', 'render', [
            ('struct drm_radeon_info info', 'inarg'),
            ('uint32_t value', '(uint64_t)info.value')]),
        ('DRM_IOCTL_GET_CAP', 'modeset', [
            ('struct drm_get_cap cap', 'inarg')]),
        ('DRM_IOCTL_MODE_CREATE_DUMB', 'render', [
            ('struct drm_mode_create_dumb dumb', 'inarg')]),
        ('DRM_IOCTL_MODE_DESTROY_DUMB', 'render', [
            ('struct drm_mode_destroy_dumb dumb', 'inarg')]),
        ('DRM_IOCTL_MODE_ADDFB', 'render', [
            ('struct drm_mode_fb_cmd fb', 'inarg')]),
        ('DRM_IOCTL_MODE_RMFB', 'render', [
            ('struct drm_mode_fb_cmd fb', 'inarg')]),
        ('DRM_IOCTL_MODE_GETCRTC', 'modeset', [
            ('struct drm_mode_crtc crtc', 'inarg'),
            ('uint32_t connectors[1024]', '(uint64_t)crtc.set_connectors_ptr', 'crtc.count_connectors * 4')]),
        ('DRM_IOCTL_SET_MASTER', 'modeset', []),
        ('DRM_IOCTL_DROP_MASTER', 'modeset', []),
        ('DRM_IOCTL_MODE_SETGAMMA', 'modeset', []),
        ('DRM_IOCTL_MODE_SETCRTC', 'modeset', []),
        ('DRM_IOCTL_RADEON_GEM_INFO', 'render', [
            ('struct drm_radeon_gem_info info', 'inarg')]),
        ('DRM_IOCTL_RADEON_GEM_CREATE', 'render', [
            ('struct drm_radeon_gem_create create', 'inarg')]),
        ('DRM_IOCTL_GEM_CLOSE', 'render', [
            ('struct drm_gem_close close', 'inarg')]),
        ('DRM_IOCTL_RADEON_GEM_MMAP', 'render', [
            ('struct drm_radeon_gem_mmap mmap', 'inarg')]),
        ('DRM_IOCTL_RADEON_GEM_SET_TILING', 'render', [
            ('struct drm_radeon_gem_set_tiling tiling', 'inarg')]),
        ('DRM_IOCTL_RADEON_GEM_BUSY', 'render', [
            ('struct drm_radeon_gem_busy busy', 'inarg')]),
        ('DRM_IOCTL_RADEON_GEM_WAIT_IDLE', 'render', [
            ('struct drm_radeon_gem_wait_idle busy', 'inarg')]),

        # Contains an array of variable-length elements. Too complex for me to
        # implement here, and the only example of that. Just do it in C. See
        # ioctls_parse_drm_ioctl_radeon_cs() in card.c.
        ('DRM_IOCTL_RADEON_CS', 'special', [],
"""	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;
	int last_retry = 0;

	if(len == 0) goto checkpoint_0;

	struct drm_radeon_cs cs;
	memcpy(&cs, buf, sizeof cs);
	char *cs_buf_ptr = buf;
	buf += sizeof cs;
	len -= sizeof cs;
	if(cs.num_chunks != 0 && len == 0) goto checkpoint_1;

	uint64_t chunk_ptrs[4096];
	if(cs.num_chunks > sizeof chunk_ptrs / sizeof chunk_ptrs[0]) goto err;
	memcpy(chunk_ptrs, buf, 8 * cs.num_chunks);
	char *chunk_ptrs_buf_ptr = buf;
	buf += 8 * cs.num_chunks;
	len -= 8 * cs.num_chunks;
	if(cs.num_chunks != 0 && len == 0) goto checkpoint_2;

	struct drm_radeon_cs_chunk chunks[
		sizeof chunk_ptrs / sizeof chunk_ptrs[0]];
	if(cs.num_chunks > sizeof chunks / sizeof chunks[0]) goto err;
	memcpy(chunks, buf, sizeof chunks[0] * cs.num_chunks);
	char *chunks_buf_ptr = buf;
	buf += sizeof chunks[0] * cs.num_chunks;
	len -= sizeof chunks[0] * cs.num_chunks;

	size_t chunks_data_sz = 0;
	int i;
	for(i = 0; i < cs.num_chunks; ++i) {
		chunks_data_sz += chunks[i].length_dw * 4;
	}
	if(chunks_data_sz != 0 && len == 0) goto checkpoint_3;
	char chunk_data[0x21000];
	if(chunks_data_sz > sizeof chunk_data) goto err;
	memcpy(chunk_data, buf, chunks_data_sz);
	char *chunk_data_buf_ptr = buf;
	buf += chunks_data_sz;
	len -= chunks_data_sz;

	if(!outsize) goto checkpoint_3;

	uint64_t real_chunks = cs.chunks;
	cs.chunks = (uint64_t)&chunk_ptrs;

	uint64_t real_chunk_ptrs[sizeof chunk_ptrs / sizeof chunk_ptrs[0]];
	for(i = 0; i < cs.num_chunks; ++i) {
		real_chunk_ptrs[i] = chunk_ptrs[i];
		chunk_ptrs[i] = (uint64_t)&chunks[i];
	}

	uint64_t real_chunk_datas[sizeof chunk_ptrs / sizeof chunk_ptrs[0]];
	unsigned pos = 0;
	for(i = 0; i < cs.num_chunks; ++i) {
		real_chunk_datas[i] = chunks[i].chunk_data;
		chunks[i].chunk_data = (uint64_t)(chunk_data + pos);
		pos += chunks[i].length_dw * 4;
	}

	unsigned real_num_chunks = cs.num_chunks;
	int status = ioctls_render(user, DRM_IOCTL_RADEON_CS, &cs);

	memcpy(chunk_data_buf_ptr, chunk_data, chunks_data_sz);

	for(i = 0; i < cs.num_chunks; ++i) {
		chunks[i].chunk_data = real_chunk_datas[i];
	}
	memcpy(chunks_buf_ptr, chunks, sizeof chunks[0] * real_num_chunks);

	for(i = 0; i < cs.num_chunks; ++i) {
		chunk_ptrs[i] = real_chunk_ptrs[i];
	}
	memcpy(chunk_ptrs_buf_ptr, chunk_ptrs, 8 * real_num_chunks);

	cs.chunks = real_chunks;
	memcpy(cs_buf_ptr, &cs, sizeof cs);

	/* Sizeof cs instead of len0 because of readonly thing. */
	ioctls_render_done(user, status, unique, DRM_IOCTL_RADEON_CS,
			arg, buf0, sizeof cs);
	return;

	struct fuse_ioctl_iovec fiov[2 + 2 * sizeof chunk_ptrs /
		sizeof chunk_ptrs[0]];
checkpoint_3:
	last_retry = 1;
	for(i = 0; i < cs.num_chunks; ++i) {
		unsigned pos = 2 + cs.num_chunks + i;
		fiov[pos].base = chunks[i].chunk_data;
		fiov[pos].len = chunks[i].length_dw * 4;
		++n_iovs;
	}
checkpoint_2:
	for(i = 0; i < cs.num_chunks; ++i) {
		unsigned pos = 2 + i;
		fiov[pos].base = chunk_ptrs[i];
		fiov[pos].len = sizeof chunks[0];
		++n_iovs;
	}
checkpoint_1:
	fiov[1].base = cs.chunks;
	fiov[1].len = 8 * cs.num_chunks;
	++n_iovs;
checkpoint_0:
	fiov[0].base = arg;
	fiov[0].len = sizeof cs;
	++n_iovs;

	ioctls_retry(user, unique, fiov, n_iovs, last_retry);
	return;

err:
	ioctls_return_error(user, unique, DRM_IOCTL_RADEON_CS,
			arg, buf0, len0);"""),

        ('DRM_IOCTL_MODE_PAGE_FLIP', 'modeset', [
            ('struct drm_mode_crtc_page_flip flip', 'inarg')]),
]

ioctls_c = open('setuid/ioctls.c', 'w')

ioctls_c.write(
        '/* Automatically generated */\n'
        '#include "ioctls.h"\n'
        '#include "../drm.h"\n'
        '#include "../radeon_drm.h"\n'
        '#include <linux/fuse.h>\n'
        '#include <string.h>\n'
        '\n')

def save_ptr(out, i):
    out.write('\tsize_t %s_actual_size = %s;\n'
            '\tvoid *%s_actual_ptr = %s%s;\n'
            '\t%s = %s%s;\n'
            '\n' % (i[1][0], i[1][2],
                i[1][0], i[1][6], i[1][5],
                i[1][5], i[1][7], i[1][1]))

def restore_ptr(out, i):
    out.write('\tmemcpy(%s_buf_ptr, %s, %s_actual_size);\n'
            '\t%s = %s%s_actual_ptr;\n'
            '\n' % (i[1][0], i[1][1], i[1][0],
                i[1][5], i[1][7], i[1][0]))

ioctl_args = []
for ioctl in ioctls:
    arg_n = 0
    have_args = set()
    goto_targets = set()
    args_processed = []

    for i in ioctl[2]:
        size = None
        ptr = None

        m = re.search('(\w*) (.*)', i[0])
        declspec = m.group(1)
        if declspec == 'struct':
            m = re.search('(struct \w*) (.*)', i[0])
            declspec = m.group(1)
            declarator = m.group(2)
        else:
            declarator = m.group(2)

        m = re.search('(\w*)\[(\d+)\]', declarator)
        if m is not None:
            name = m.group(1)
            ptr = name
            size = 'sizeof(%s) * %s' % (declspec, m.group(2))
        else:
            name = declarator
            ptr = '&%s' % (name)
            size = 'sizeof(%s)' % (declspec)

        maxsize = None
        if len(i) >= 3:
            maxsize = size
            size = i[2]

        m = re.search('(\(\w+\))(.+)', i[1])
        if m is not None:
            value_minus_type = m.group(2)
            value_to_void_ptr = '(void *)'
            value_from_void_ptr = m.group(1)
        else:
            value_minus_type = i[1]
            value_to_void_ptr = ''
            value_from_void_ptr = ''

        dep = re.search('(\w*).*', value_minus_type).group(1)

        args_processed.append((name, ptr, size, dep, maxsize, value_minus_type,
            value_to_void_ptr, value_from_void_ptr))

    ioctl_args.append(args_processed)

    ioctls_c.write('static void parse_%s(void *user, uint64_t unique,\n'
            '\t\tuint64_t arg, char *buf, int len, int outsize)\n'
            '{\n' % (ioctl[0].lower()))

    if ioctl[1] == 'special':
        ioctls_c.write('%s\n}\n\n' % (ioctl[3]))
        continue

    # Ioctls with no arguments (like DRM_IOCTL_SET_MASTER)
    if len(ioctl[2]) == 0:
        if ioctl[1] == 'modeset':
            ioctls_c.write('\tioctls_modeset(user, unique, %s, arg, NULL, '
                    '0);\n' % (ioctl[0]))
        else:
            print 'RENDER IOCTLS WITHOUT ARGUMENTS NOT HANDLED YET'
            exit(1)
        ioctls_c.write('}\n\n')
        continue

    ioctls_c.write('\tvoid *inarg = (void *)arg;\n')
    ioctls_c.write('\tchar *buf0 = buf;\n\tint len0 = len;\n')
    ioctls_c.write('\tint n_iovs = 0;\n')
    ioctls_c.write('\n')

    # For modesetting we don't actually need the data, we just send the whole
    # buffer to the library side. Don't waste time parsing structs that don't
    # contain pointer information.
    depended_upon = set()
    subsequent_buf_reads = set()
    if ioctl[1] == 'modeset':
        for i in args_processed:
            depended_upon.add(i[3])
        # Modesets always end with a if(!len)... statement so need buf -= ...
        # for all reads.
        subsequent_buf_reads = depended_upon
    else:
        depended_upon.update([i[0] for i in args_processed])
        subsequent_buf_reads.update([i[0] for i in args_processed[:-1]])

    checkpoints = 0
    first_retry = True
    prev_name = None
    for i in zip(ioctl[2], args_processed):
        if not i[1][3] in have_args:
            if len(i[0]) > 2:
                ioctls_c.write('\tif(%s != 0 && len == 0) '
                        'goto checkpoint_%d;\n' % (i[0][2], checkpoints))
            else:
                ioctls_c.write('\tif(len == 0) '
                        'goto checkpoint_%d;\n' % (checkpoints))
            checkpoints += 1
            have_args.add(i[1][3])
            if first_retry: first_retry = False
            else: goto_targets.add(prev_name)

        if i[1][0] in depended_upon:
            ioctls_c.write('\t%s;\n' % (i[0][0]))
            if ioctl[1] == 'render':
                ioctls_c.write('\tchar *%s_buf_ptr = buf;\n' % (i[1][0]))
            ioctls_c.write('\tmemcpy(%s, buf, %s);\n' % (i[1][1], i[1][2]))

        if i[1][0] in subsequent_buf_reads:
            ioctls_c.write('\tbuf += %s;\n\tlen -= %s;\n' % (i[1][2],
                i[1][2]))

        arg_n += 1
        prev_name = i[1][0]

    goto_targets.add(args_processed[-1][0])

    ioctls_c.write('\n'
            '\tif(outsize == 0) goto checkpoint_%d;\n'
            '\n' % (checkpoints - 1))
    if ioctl[1] == 'modeset':
        ioctls_c.write('\tioctls_modeset(user, unique, %s,\n'
                '\t\t\targ, buf0, len0);\n' % (ioctl[0]))
    else:
        for i in zip(ioctl[2], args_processed):
            save_ptr(ioctls_c, i)

        ioctls_c.write('\tint return_value = ioctls_render(user, %s, inarg);\n'
                % (ioctl[0]))

        for i in zip(ioctl[2], args_processed)[::-1]:
            restore_ptr(ioctls_c, i)

        ioctls_c.write('\tioctls_render_done(user, return_value, unique, %s,\n'
                '\t\t\targ, buf0, len0);\n' % (ioctl[0]))

    ioctls_c.write('\treturn;\n'
            '\n'
            '\tstruct fuse_ioctl_iovec fiov[%d];\n' % (arg_n))

    iov_pos = arg_n
    need_error_return = False
    for i in zip(ioctl[2], args_processed)[::-1]:
        iov_pos -= 1
        if i[1][0] in goto_targets:
            checkpoints -= 1
            ioctls_c.write('checkpoint_%d:\n' % (checkpoints))
        if i[1][4] is not None:
            need_error_return = True
            ioctls_c.write('\tif(%s > %s) goto return_error;\n' %
                    (i[0][2], i[1][4]))
        ioctls_c.write('\tfiov[%d].base = (uint64_t)%s;\n'
                '\tfiov[%d].len = %s;\n'
                '\t++n_iovs;\n' % (iov_pos, i[1][5], iov_pos, i[1][2]))

    ioctls_c.write('\tioctls_retry(user, unique, fiov, n_iovs, '
            'n_iovs == %d ? n_iovs : 0);\n' % (arg_n))

    if need_error_return:
        ioctls_c.write(
                '\treturn;\n'
                '\n'
                'return_error:\n'
                '\tioctls_return_error(user, unique, %s,\n'
                '\t\t\targ, buf0, len0);\n' % (ioctl[0]))

    ioctls_c.write('}\n\n')

ioctls_c.write(
        'void ioctls_handle(void *user, uint64_t unique, uint32_t cmd,\n'
        '\t\tuint64_t arg, char *buf, int len, int outsize)\n'
        '{\n'
        '\tswitch(cmd) {\n')

for ioctl in zip(ioctls, ioctl_args):
    ioctls_c.write('\t\tcase %s:\n'
            '\t\t\tparse_%s(\n'
            '\t\t\t\tuser, unique, arg, buf, len, outsize);\n'
            '\t\t\tbreak;\n' % (
                ioctl[0][0],
                ioctl[0][0].lower()))

ioctls_c.write(
        '\t\tdefault:\n'
        '\t\t\tioctls_return_error(user, unique, cmd, arg, buf, len);\n'
        '\t}\n'
        '}\n')

ioctls_c.close()

ioctls_c = open('lib/ioctls.c', 'w')

ioctls_c.write('/* Automatically generated */\n'
        '#include "ioctls.h"\n'
        '#include <stdlib.h>\n'
        '#include <string.h>\n'
        '#include "../drm.h"\n'
        '#include "../radeon_drm.h"\n'
        '\n')
for ioctl in zip(ioctls, ioctl_args):
    if ioctl[0][1] != 'modeset': continue

    ioctls_c.write('static int parse_%s(char *buf, int len, void *user)\n'
            '{\n'
            '\tvoid *inarg = NULL;\n'
            '\n' % (ioctl[0][0].lower()))
    for i in zip(ioctl[0][2], ioctl[1]):
        ioctls_c.write('\t%s;\n'
                '\tmemcpy(%s, buf, %s);\n'
                '\tchar *%s_buf_ptr = buf;\n'
                '\tbuf += %s;\n'
                '\tlen -= %s;\n'
                % (i[0][0],
                    i[1][1], i[1][2],
                    i[1][0],
                    i[1][2],
                    i[1][2]))
        save_ptr(ioctls_c, i)

    ioctls_c.write('\tint return_value = ioctls_process(user, %s, inarg);\n'
            '\n' % (ioctl[0][0]))

    for i in zip(ioctl[0][2], ioctl[1])[::-1]:
        restore_ptr(ioctls_c, i)

    ioctls_c.write('\treturn return_value;\n'
            '}\n'
            '\n')

ioctls_c.write('int ioctls_parse_and_process(uint32_t cmd, char *buf, '
        'int len, void *user)\n'
        '{\n'
        '\tswitch(cmd) {\n')

for ioctl in zip(ioctls, ioctl_args):
    if ioctl[0][1] != 'modeset': continue
    ioctls_c.write('\t\tcase %s:\n'
            '\t\t\treturn parse_%s(buf, len, user);\n' % (ioctl[0][0],
                ioctl[0][0].lower()))

ioctls_c.write('\t}\n'
        '\treturn -1;\n'
        '}\n')

ioctls_c.close()
