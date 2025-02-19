/*
 * Copyright 2014-2019 Real Logic Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "aeron_properties_util.h"
#include "aeron_error.h"
#include "aeron_parse_util.h"
#include "aeron_http_util.h"

int aeron_next_non_whitespace(const char *buffer, size_t start, size_t end)
{
    for (size_t i = start; i <= end; i++)
    {
        const char c = buffer[i];

        if (c == ' ' || c == '\t')
        {
            continue;
        }

        return '\0' == c ? -1 : (int)i;
    }

    return -1;
}

/*
 * Format taken from
 * https://docs.oracle.com/cd/E23095_01/Platform.93/ATGProgGuide/html/s0204propertiesfileformat01.html
 */
int aeron_properties_parse_line(
    aeron_properties_parser_state_t *state,
    const char *line,
    size_t length,
    aeron_properties_file_handler_func_t handler,
    void *clientd)
{
    bool in_name = 0 < state->name_end ? false : true;
    int value_start = 0, result = 0;

    if (length >= (sizeof(state->property_str) - state->value_end))
    {
        aeron_set_err(EINVAL, "line length " PRIu64 " too long for parser state", (uint64_t)(length + state->value_end));
        return -1;
    }

    if (in_name)
    {
        int cursor = aeron_next_non_whitespace(line, 0, length - 1);

        if (-1 == cursor || '!' == line[cursor] || '#' == line[cursor])
        {
            return 0;
        }

        for (size_t i = cursor; i < length; i++)
        {
            const char c = line[i];

            if (':' == c || '=' == c)
            {
                state->property_str[state->name_end] = '\0';
                value_start = i + 1;

                /* trim back for whitespace after name */
                for (int j = i - 1; j >= 0; j--)
                {
                    if (' ' != line[j] && '\t' != line[j])
                    {
                        break;
                    }

                    state->property_str[--state->name_end] = '\0';
                }

                state->value_end = state->name_end + 1;
                break;
            }

            state->property_str[state->name_end++] = c;
        }

        if (0 == state->value_end || 0 == state->name_end)
        {
            aeron_set_err(EINVAL, "%s", "malformed line");
            aeron_properties_parse_init(state);
            return -1;
        }

        value_start = aeron_next_non_whitespace(line, value_start, length - 1);

        if (-1 == value_start)
        {
            state->property_str[state->value_end++] = '\0';

            result = handler(clientd, state->property_str, state->property_str + state->name_end + 1);

            aeron_properties_parse_init(state);
            return result;
        }
    }
    else
    {
        value_start = aeron_next_non_whitespace(line, value_start, length - 1);

        if (-1 == value_start || '!' == line[value_start] || '#' == line[value_start])
        {
            return 0;
        }
    }

    if ('\\' == line[length - 1])
    {
        memcpy(state->property_str + state->value_end, line + value_start, length - value_start - 1);
        state->value_end += length - value_start - 1;
    }
    else
    {
        memcpy(state->property_str + state->value_end, line + value_start, length - value_start);
        state->value_end += length - value_start;
        state->property_str[state->value_end++] = '\0';

        result = handler(clientd, state->property_str, state->property_str + state->name_end + 1);

        aeron_properties_parse_init(state);
    }

    return result;
}

int aeron_properties_setenv(const char *name, const char *value)
{
    char env_name[AERON_PROPERTIES_MAX_LENGTH];

    for (size_t i = 0; i < sizeof(env_name); i++)
    {
        const char c = name[i];

        if ('.' == c)
        {
            env_name[i] = '_';
        }
        else if ('\0' == c)
        {
            env_name[i] = c;
            break;
        }
        else
        {
            env_name[i] = toupper(c);
        }
    }

    if ('\0' == *value)
    {
#if !defined(WIN32)
        unsetenv(env_name);
#else
        _putenv_s(env_name, "");
#endif
    }
    else
    {
#if !defined(WIN32)
        setenv(env_name, value, true);
#else
        _putenv_s(env_name, value);
#endif
    }

    return 0;
}

int aeron_properties_setenv_property(void *clientd, const char *name, const char *value)
{
    return aeron_properties_setenv(name, value);
}

int aeron_properties_file_load(const char *filename)
{
    FILE *fpin;
    int result = -1, lineno = 1;
    char line[AERON_PROPERTIES_MAX_LENGTH];
    aeron_properties_parser_state_t state;

    if ((fpin = fopen(filename, "r")) == NULL)
    {
        aeron_set_err(EINVAL, "could not open filename %s", filename);
        return -1;
    }

    aeron_properties_parse_init(&state);

    while (fgets(line, sizeof(line), fpin) != NULL)
    {
        size_t length = strlen(line);

        if ('\n' == line[length - 1])
        {
            line[length - 1] = '\0';
            length--;

            if ('\r' == line[length - 1])
            {
                line[length - 1] = '\0';
                length--;
            }

            if (aeron_properties_parse_line(&state, line, length, aeron_properties_setenv_property, NULL) < 0)
            {
                aeron_set_err(EINVAL, "properties file line %" PRId32 " malformed", lineno);
                goto cleanup;
            }
        }
        else
        {
            aeron_set_err(EINVAL, "properties file line %" PRId32 " too long or does not end with newline", lineno);
            goto cleanup;
        }

        lineno++;
    }

    if (!feof(fpin))
    {
        int err_code = errno;

        aeron_set_err(err_code, "error reading file: %s", strerror(err_code));
        goto cleanup;
    }
    else
    {
        result = 0;
    }

    cleanup:
    fclose(fpin);

    return result;
}

int aeron_properties_buffer_load(const char *buffer)
{
    char line[AERON_PROPERTIES_MAX_LENGTH];
    int line_length = 0, cursor = 0, lineno = 1;
    aeron_properties_parser_state_t state;

    aeron_properties_parse_init(&state);

    while ((line_length = aeron_parse_get_line(line, sizeof(line), buffer + cursor)) > 0)
    {
        cursor += line_length;

        if ('\n' == line[line_length - 1])
        {
            line[line_length - 1] = '\0';
            line_length--;

            if ('\r' == line[line_length - 1])
            {
                line[line_length - 1] = '\0';
                line_length--;
            }

            if (aeron_properties_parse_line(&state, line, line_length, aeron_properties_setenv_property, NULL) < 0)
            {
                aeron_set_err(EINVAL, "properties buffer line %" PRId32 " malformed", lineno);
                return -1;
            }
        }
        else
        {
            aeron_set_err(EINVAL, "properties buffer line %" PRId32 " too long or does not end with newline", lineno);
            return -1;
        }

        lineno++;
    }

    if (line_length < 0)
    {
        return -1;
    }

    return 0;
}

int aeron_properties_http_load(const char *url)
{
    char new_url[AERON_MAX_HTTP_URL_LENGTH];
    aeron_http_response_t *response = NULL;
    int remaining_redirects = 1, result = -1;

    do
    {
        if (aeron_http_retrieve(&response, url, AERON_HTTP_PROPERTIES_TIMEOUT_NS) < 0)
        {
            return -1;
        }

        if (200 == response->status_code)
        {
            break;
        }
        else if (301 == response->status_code || 302 == response->status_code)
        {
            if (remaining_redirects-- > 0)
            {

                switch (aeron_http_header_get(response, "Location:", new_url, sizeof(new_url)))
                {
                    case -1:
                        goto cleanup;

                    case 0:
                        aeron_set_err(EINVAL, "%s", "redirect specified, but no Location header found");
                        goto cleanup;

                    default:
                    {
                        url = new_url + strlen("Location:");

                        while ('\0' != *url && (' ' == *url || '\t' == *url))
                        {
                            url++;
                        }

                        aeron_http_response_delete(response);
                        response = NULL;
                        break;
                    }
                }
            }
            else
            {
                aeron_set_err(EINVAL, "%s", "too many redirects for URL");
                goto cleanup;
            }
        }
        else
        {
            aeron_set_err(EINVAL, "status code %" PRIu32 " from HTTP GET", (uint32_t)response->status_code);
            goto cleanup;
        }
    }
    while (true);

    result = aeron_properties_buffer_load(response->buffer + response->body_offset);

    cleanup:
        aeron_http_response_delete(response);

        return result;
}

int aeron_properties_load(const char *url_or_filename)
{
    int result = -1;

    if (strncmp("file://", url_or_filename, strlen("file://")) == 0)
    {
        result = aeron_properties_file_load(url_or_filename + strlen("file://"));
    }
    else if (strncmp("http://", url_or_filename, strlen("http://")) == 0)
    {
        result = aeron_properties_http_load(url_or_filename);
    }
    else
    {
        result = aeron_properties_file_load(url_or_filename);
    }

    return result;
}

extern void aeron_properties_parse_init(aeron_properties_parser_state_t *state);
