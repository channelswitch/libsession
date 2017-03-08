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
            ('struct drm_radeon_info info', 'inarg')])
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
        type_word = i[0].split(' ')[0]
        size = None
        ptr = None
        array_match = re.search('(\w+) (\w+)\[(\d+)\]', i[0])
        if type_word == 'struct':
            m = re.search('struct (\w*) (\w*)', i[0])
            name = m.group(2)
            size = 'sizeof(struct %s)' % (m.group(1))
        elif array_match is not None:
            m = array_match
            ttype = m.group(1)
            name = m.group(2)
            size = m.group(3)
            ptr = name
        else:
            raise Exception('Unknown type')

        if ptr == None: ptr = '&%s' % (name)
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

    ioctls_c.write('static void parse_%s(void *user, uint64_t unique,\n'
            '\t\tuint64_t arg, char *buf, int len, int outsize)\n'
            '{\n' % (ioctl[0].lower()))
    ioctls_c.write('\tvoid *inarg = (void *)arg;\n')
    ioctls_c.write('\tchar *buf0 = buf;\n\tint len0 = len;\n')
    ioctls_c.write('\tint n_iovs = 0;\n')

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
                '\t\t\targ, buf, len);\n' % (ioctl[0]))

#        ioctls_c.write('\t%s_to_real_card(inarg, %s);\n' %
#                (ioctl[0].lower(), ', '.join([i[0] for i in args_processed])))

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
            'n_iovs == %d);\n' % (arg_n))

    if need_error_return:
        ioctls_c.write(
                '\treturn;\n'
                '\n'
                'return_error:\n'
                '\tioctls_return_error(user, unique, %s,\n'
                '\t\t\targ, buf0, len0);\n' % (ioctl[0]))

    ioctls_c.write('}\n\n')

    ioctl_args.append(args_processed)

ioctls_c.write(
        'void ioctls_handle(void *user, uint64_t unique, uint32_t cmd,\n'
        '\t\tuint64_t arg, char *buf, int len, int outsize)\n'
        '{\n'
        '\tswitch(cmd) {\n')

for ioctl in zip(ioctls, ioctl_args):
    ioctls_c.write('\t\tcase %s:\n'
            '\t\t\tparse_%s(\n'
            '\t\t\t\tuser, unique, arg, buf, len, outsize);\n'
            '\t\t\tbreak;\n' % (ioctl[0][0], ioctl[0][0].lower()))

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
    if ioctl[0][1] == 'render': continue

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
    if ioctl[0][1] == 'render': continue
    ioctls_c.write('\t\tcase %s:\n'
            '\t\t\treturn parse_%s(buf, len, user);\n' % (ioctl[0][0],
                ioctl[0][0].lower()))

ioctls_c.write('\t}\n'
        '\treturn -1;\n'
        '}\n')

ioctls_c.close()
