#include "shared.h"

// GET / HTTP/1.1\r\n
void parse_req_headline(headers_parser_state *state, req_headline *hline) {
    char *line = state->line;
    char *pos = memchr(line, '\r', state->rest_len);

    if (unlikely(!pos || *(pos + 1) != '\n')) {
        return; // no headline
    }

    int line_len = pos - line;
    pos = memchr(line, ' ', line_len);

    if (likely(pos != NULL)) {
        hline->method = line;
        hline->method_len = pos - line;
        hline->path = pos + 1;
        pos = memchr(pos + 1, ' ', line_len - (pos + 1 - line));

        if (likely(pos != NULL)) {
            hline->path_len = pos - hline->path;
            hline->ver = pos + 1;
            int len = line_len - (pos + 1 - line);
            hline->ver_len = unlikely(len < 0) ? 0 : len;
        }
    }

    state->line += line_len + 2; // +2 for \r\n
    state->rest_len -= line_len + 2; // +2 for \r\n
}

// HTTP/1.1 200 OK\r\n
void parse_res_headline(headers_parser_state *state, res_headline *hline) {
    char *line = state->line;
    char *pos = memchr(line, '\r', state->rest_len);

    if (unlikely(!pos || *(pos + 1) != '\n')) {
        return; // no headline
    }

    int line_len = pos - line;
    pos = memchr(line, ' ', line_len);

    if (likely(pos != NULL)) {
        hline->ver = line;
        hline->ver_len = pos - line;
        pos++;
        hline->code = parse_dec(&pos);
        hline->msg = pos + 1;
        int len = line_len - (pos + 1 - line);
        hline->msg_len = unlikely(len < 0) ? 0 : len;
    }

    state->line += line_len + 2; // +2 for \r\n
    state->rest_len -= line_len + 2; // +2 for \r\n
}

void parse_headers(lua_State *L, headers_parser_state *state) {
    while (1) {
        char *pos = memchr(state->line, '\r', state->rest_len);

        if (unlikely(!pos || *(pos + 1) != '\n')) {
            break;
        }

        int line_len = pos - state->line + 2; // +2 for \r\n
        state->rest_len -= line_len;

        if (line_len == 2) { // \r\n\r\n
            state->line += 2;
            return; // end of headers
        }

        pos = memchr(state->line, ':', line_len);

        if (pos) {
            int k_len = pos - state->line;

            lua_pushlstring(L, state->line, k_len); // k
            lua_pushlstring(L, pos + 2, line_len - k_len - 2 - 2); // v

            if (strcasecmp(lua_tostring(L, -2), HTTP_HDR_TRANSFER_ENC) == 0) {
                state->is_chunked =
                    strcasecmp(lua_tostring(L, -1), HTTP_CHUNKED) == 0;
            }

            if (strcasecmp(lua_tostring(L, -2), HTTP_HDR_CONTENT_LEN) == 0) {
                pos = (char *)lua_tostring(L, -1); // will not be changed
                state->content_len = parse_dec(&pos);
            }

            lua_rawset(L, -3);
        } else {
            lua_pushlstring(L, state->line, line_len - 2);
            lua_setfield(L, -2, "");
        }

        state->line += line_len;
    }
}

int ssl_error(lua_State *L, const char *fn_name) {
    int errors_n = ssl_warn_err_stack(L);
    return luaL_error(L, "%s failed; errors: %d", fn_name, errors_n);
}

int ssl_error_ret(lua_State *L, const char *fn_name, SSL *ssl, int ret) {
    int errors_n = ssl_warn_err_stack(L);
    int err_code = SSL_get_error(ssl, ret);
    const char *err_label = "unknown error code";

    #define CASE(value) { case value: err_label = #value; break; }

    switch (err_code) {
        CASE(SSL_ERROR_NONE);
        CASE(SSL_ERROR_SSL);
        CASE(SSL_ERROR_WANT_READ);
        CASE(SSL_ERROR_WANT_WRITE);
        CASE(SSL_ERROR_WANT_X509_LOOKUP);
        CASE(SSL_ERROR_SYSCALL);
        CASE(SSL_ERROR_ZERO_RETURN);
        CASE(SSL_ERROR_WANT_CONNECT);
        CASE(SSL_ERROR_WANT_ACCEPT);
        CASE(SSL_ERROR_WANT_ASYNC);
        CASE(SSL_ERROR_WANT_ASYNC_JOB);
        CASE(SSL_ERROR_WANT_CLIENT_HELLO_CB);
        CASE(SSL_ERROR_WANT_RETRY_VERIFY);
    }

    #undef CASE

    return luaL_error(L, "%s failed; code: %s (%d); errors n: %d",
        fn_name, err_label, err_code, errors_n);
}

int ssl_error_verify(lua_State *L, long result) {
    const char *result_label = "unknown result";

    #define CASE(value) { case value: result_label = #value; break; }

    switch (result) {
        CASE(X509_V_OK);
        CASE(X509_V_ERR_UNSPECIFIED);
        CASE(X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT);
        CASE(X509_V_ERR_UNABLE_TO_GET_CRL);
        CASE(X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE);
        CASE(X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE);
        CASE(X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY);
        CASE(X509_V_ERR_CERT_SIGNATURE_FAILURE);
        CASE(X509_V_ERR_CRL_SIGNATURE_FAILURE);
        CASE(X509_V_ERR_CERT_NOT_YET_VALID);
        CASE(X509_V_ERR_CERT_HAS_EXPIRED);
        CASE(X509_V_ERR_CRL_NOT_YET_VALID);
        CASE(X509_V_ERR_CRL_HAS_EXPIRED);
        CASE(X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD);
        CASE(X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD);
        CASE(X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD);
        CASE(X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD);
        CASE(X509_V_ERR_OUT_OF_MEM);
        CASE(X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT);
        CASE(X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN);
        CASE(X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY);
        CASE(X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE);
        CASE(X509_V_ERR_CERT_CHAIN_TOO_LONG);
        CASE(X509_V_ERR_CERT_REVOKED);
        CASE(X509_V_ERR_NO_ISSUER_PUBLIC_KEY);
        CASE(X509_V_ERR_PATH_LENGTH_EXCEEDED);
        CASE(X509_V_ERR_INVALID_PURPOSE);
        CASE(X509_V_ERR_CERT_UNTRUSTED);
        CASE(X509_V_ERR_CERT_REJECTED);
        CASE(X509_V_ERR_SUBJECT_ISSUER_MISMATCH);
        CASE(X509_V_ERR_AKID_SKID_MISMATCH);
        CASE(X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH);
        CASE(X509_V_ERR_KEYUSAGE_NO_CERTSIGN);
        CASE(X509_V_ERR_UNABLE_TO_GET_CRL_ISSUER);
        CASE(X509_V_ERR_UNHANDLED_CRITICAL_EXTENSION);
        CASE(X509_V_ERR_KEYUSAGE_NO_CRL_SIGN);
        CASE(X509_V_ERR_UNHANDLED_CRITICAL_CRL_EXTENSION);
        CASE(X509_V_ERR_INVALID_NON_CA);
        CASE(X509_V_ERR_PROXY_PATH_LENGTH_EXCEEDED);
        CASE(X509_V_ERR_KEYUSAGE_NO_DIGITAL_SIGNATURE);
        CASE(X509_V_ERR_PROXY_CERTIFICATES_NOT_ALLOWED);
        CASE(X509_V_ERR_INVALID_EXTENSION);
        CASE(X509_V_ERR_INVALID_POLICY_EXTENSION);
        CASE(X509_V_ERR_NO_EXPLICIT_POLICY);
        CASE(X509_V_ERR_DIFFERENT_CRL_SCOPE);
        CASE(X509_V_ERR_UNSUPPORTED_EXTENSION_FEATURE);
        CASE(X509_V_ERR_UNNESTED_RESOURCE);
        CASE(X509_V_ERR_PERMITTED_VIOLATION);
        CASE(X509_V_ERR_EXCLUDED_VIOLATION);
        CASE(X509_V_ERR_SUBTREE_MINMAX);
        CASE(X509_V_ERR_APPLICATION_VERIFICATION);
        CASE(X509_V_ERR_UNSUPPORTED_CONSTRAINT_TYPE);
        CASE(X509_V_ERR_UNSUPPORTED_CONSTRAINT_SYNTAX);
        CASE(X509_V_ERR_UNSUPPORTED_NAME_SYNTAX);
        CASE(X509_V_ERR_CRL_PATH_VALIDATION_ERROR);
        CASE(X509_V_ERR_PATH_LOOP);
        CASE(X509_V_ERR_SUITE_B_INVALID_VERSION);
        CASE(X509_V_ERR_SUITE_B_INVALID_ALGORITHM);
        CASE(X509_V_ERR_SUITE_B_INVALID_CURVE);
        CASE(X509_V_ERR_SUITE_B_INVALID_SIGNATURE_ALGORITHM);
        CASE(X509_V_ERR_SUITE_B_LOS_NOT_ALLOWED);
        CASE(X509_V_ERR_SUITE_B_CANNOT_SIGN_P_384_WITH_P_256);
        CASE(X509_V_ERR_HOSTNAME_MISMATCH);
        CASE(X509_V_ERR_EMAIL_MISMATCH);
        CASE(X509_V_ERR_IP_ADDRESS_MISMATCH);
        CASE(X509_V_ERR_DANE_NO_MATCH);
        CASE(X509_V_ERR_EE_KEY_TOO_SMALL);
        CASE(X509_V_ERR_CA_KEY_TOO_SMALL);
        CASE(X509_V_ERR_CA_MD_TOO_WEAK);
        CASE(X509_V_ERR_INVALID_CALL);
        CASE(X509_V_ERR_STORE_LOOKUP);
        CASE(X509_V_ERR_NO_VALID_SCTS);
        CASE(X509_V_ERR_PROXY_SUBJECT_NAME_VIOLATION);
        CASE(X509_V_ERR_OCSP_VERIFY_NEEDED);
        CASE(X509_V_ERR_OCSP_VERIFY_FAILED);
        CASE(X509_V_ERR_OCSP_CERT_UNKNOWN);
        CASE(X509_V_ERR_UNSUPPORTED_SIGNATURE_ALGORITHM);
        CASE(X509_V_ERR_SIGNATURE_ALGORITHM_MISMATCH);
        CASE(X509_V_ERR_SIGNATURE_ALGORITHM_INCONSISTENCY);
        CASE(X509_V_ERR_INVALID_CA);
        CASE(X509_V_ERR_PATHLEN_INVALID_FOR_NON_CA);
        CASE(X509_V_ERR_PATHLEN_WITHOUT_KU_KEY_CERT_SIGN);
        CASE(X509_V_ERR_KU_KEY_CERT_SIGN_INVALID_FOR_NON_CA);
        CASE(X509_V_ERR_ISSUER_NAME_EMPTY);
        CASE(X509_V_ERR_SUBJECT_NAME_EMPTY);
        CASE(X509_V_ERR_MISSING_AUTHORITY_KEY_IDENTIFIER);
        CASE(X509_V_ERR_MISSING_SUBJECT_KEY_IDENTIFIER);
        CASE(X509_V_ERR_EMPTY_SUBJECT_ALT_NAME);
        CASE(X509_V_ERR_EMPTY_SUBJECT_SAN_NOT_CRITICAL);
        CASE(X509_V_ERR_CA_BCONS_NOT_CRITICAL);
        CASE(X509_V_ERR_AUTHORITY_KEY_IDENTIFIER_CRITICAL);
        CASE(X509_V_ERR_SUBJECT_KEY_IDENTIFIER_CRITICAL);
        CASE(X509_V_ERR_CA_CERT_MISSING_KEY_USAGE);
        CASE(X509_V_ERR_EXTENSIONS_REQUIRE_VERSION_3);
        CASE(X509_V_ERR_EC_KEY_EXPLICIT_PARAMS);
        CASE(X509_V_ERR_RPK_UNTRUSTED);
    }

    #undef CASE

    return luaL_error(L, "SSL_get_verify_result failed; code: %s (%d)",
        result_label, result);
}

int ssl_warn_err_stack(lua_State *L) {
    int errors_n = 0;
    unsigned long error_code;
    static char error_msg[256];

    while ((error_code = ERR_get_error()) != 0) {
        errors_n++;

        ERR_error_string_n(error_code, error_msg, sizeof(error_msg));

        const char *lib = ERR_lib_error_string(error_code);
        const char *reason = ERR_reason_error_string(error_code);

        luaF_warning(L, "ssl stack error #%d"
            "; code: %d; msg: %s; lib: %s; reason: %s",
            errors_n, error_code, error_msg, lib, reason);
    }

    return errors_n;
}
