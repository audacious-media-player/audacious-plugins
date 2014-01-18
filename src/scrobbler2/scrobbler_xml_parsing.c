
//plugin includes
#include "scrobbler.h"

//static (private) variables
static xmlDocPtr doc = NULL;
static xmlXPathContextPtr context = NULL;

static bool_t prepare_data () {
    received_data[received_data_size] = '\0';
    AUDDBG("Data received from last.fm:\n%s\n%%%%End of data%%%%\n", received_data);

    doc = xmlParseMemory(received_data, received_data_size+1);
    received_data_size = 0;
    if (doc == NULL) {
        AUDDBG("Document not parsed successfully.\n");
        return FALSE;
    }

    context = xmlXPathNewContext(doc);
    if (context == NULL) {
        AUDDBG("Error in xmlXPathNewContext\n");
        xmlFreeDoc(doc);
        doc = NULL;
        return FALSE;
    }
    return TRUE;
}

static void clean_data() {
    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);
    context = NULL;
    doc = NULL;
}


//returns:
// NULL if an error occurs or the attribute was not found
// the attribute's value if it was found
static char *get_attribute_value (const char *node_expression, const char *attribute) {
    if (doc == NULL || context == NULL) {
        AUDDBG("Response from last.fm not parsed successfully. Did you call prepare_data?\n");
        return NULL;
    }

    xmlXPathObjectPtr statusObj = xmlXPathEvalExpression((xmlChar *) node_expression, context);
    if (statusObj == NULL) {
        AUDDBG ("Error in xmlXPathEvalExpression.\n");
        return NULL;
    }
    if (xmlXPathNodeSetIsEmpty(statusObj->nodesetval)) {
        AUDDBG("No result.\n");
        xmlXPathFreeObject(statusObj);
        return NULL;
    }

    xmlChar *prop = xmlGetProp(statusObj->nodesetval->nodeTab[0], (xmlChar *) attribute);

    char *result = NULL;
    if (prop && prop[0])
        result = str_get((char *) prop);

    xmlXPathFreeObject(statusObj);
    xmlFree(prop);

    AUDDBG("RESULT FOR THIS FUNCTION: %s.\n", result);
    return result;
}

//returns:
// NULL if an error occurs or the node was not found
static char *get_node_string (const char *node_expression) {
    if (doc == NULL || context == NULL) {
        AUDDBG("Response from last.fm not parsed successfully. Did you call prepare_data?\n");
        return NULL;
    }

    xmlXPathObjectPtr statusObj = xmlXPathEvalExpression((xmlChar *) node_expression, context);
    if (statusObj == NULL) {
        AUDDBG ("Error in xmlXPathEvalExpression.\n");
        return NULL;
    }
    if (xmlXPathNodeSetIsEmpty(statusObj->nodesetval)) {
        AUDDBG("No result.\n");
        xmlXPathFreeObject(statusObj);
        return NULL;
    }

    xmlChar *string = xmlNodeListGetString(doc, statusObj->nodesetval->nodeTab[0]->children, 1);

    char *result = NULL;
    if (string && string[0])
        result = str_get((char *) string);

    xmlXPathFreeObject(statusObj);
    xmlFree(string);

    AUDDBG("RESULT FOR THIS FUNCTION: %s.\n", result);
    return result;
}


//returns:
// NULL if an error occurs
// "true" if the the command succeeded
// "false" if an error occurred. error_code and error_detail should be checked in this case
static char *check_status (char **error_code, char **error_detail) {
    (*error_code) = NULL;
    (*error_detail) = NULL;

    char *status = get_attribute_value("/lfm[@status]", "status");
    if (!status) {
        AUDDBG("last.fm not answering according to the API.\n");
        return NULL;
    }

    AUDDBG ("status is %s.\n", status);
    if (strcmp(status, "ok")) {

        (*error_code) = get_attribute_value("/lfm/error[@code]", "code");
        if (!(*error_code)) {
            AUDDBG("Weird API answer. Last.fm says status is %s but there is no error code?\n", status);
            str_unref(status);
            status = NULL;
        } else {
            (*error_detail) = get_node_string("/lfm/error");
        }
    }

    AUDDBG("check_status results: return: %s. error_code: %s. error_detail: %s.\n", status, (*error_code), (*error_detail));
    return status;
}

/*
 * Returns:
 *  * TRUE if the scrobble was successful
 *    * with ignored = TRUE if it was ignored
 *      * ignored_code_out must be checked
 *    * with ignored = FALSE if it was scrobbled OK
 *  * FALSE if the scrobble was unsuccessful
 *    * error_code_out and error_detail_out must be checked:
 *      * They are NULL if an API communication error occur
 */
bool_t read_scrobble_result(char **error_code, char **error_detail, bool_t *ignored, char **ignored_code) {

    *error_code = NULL;
    *error_detail = NULL;
    *ignored = FALSE;
    *ignored_code = NULL;

    bool_t result = TRUE;

    if (!prepare_data()) {
        AUDDBG("Could not read received data from last.fm. What's up?\n");
        return FALSE;
    }

    char *status = check_status(error_code, error_detail);

    if (!status) {
        AUDDBG("Status was NULL. Invalid API answer.\n");
        clean_data();
        return FALSE;
    }

    if (!strcmp(status, "failed")) {
        AUDDBG("Error code: %s. Detail: %s.\n", *error_code, *error_detail);
        result = FALSE;

    } else {
        //TODO: We are assuming that only one track is scrobbled per request! This will have to be
        //re-done to support multiple tracks being scrobbled in batch
        char *ignored_scrobble = get_attribute_value("/lfm/scrobbles[@ignored]", "ignored");

        if (ignored_scrobble && strcmp(ignored_scrobble, "0")) {
          //The track was ignored
          //TODO: this assumes ignored_scrobble == 1!!!
          *ignored = TRUE;
          *ignored_code = get_attribute_value("/lfm/scrobbles/scrobble/ignoredMessage[@code]", "code");
        }

        str_unref(ignored_scrobble);

        AUDDBG("ignored? %i, ignored_code: %s\n", *ignored, *ignored_code);
    }

    str_unref(status);

    clean_data();
    return result;
}

//returns
//FALSE if there was an error with the connection
bool_t read_authentication_test_result (char **error_code, char **error_detail) {

    *error_code = NULL;
    *error_detail = NULL;

    bool_t result = TRUE;

    if (!prepare_data()) {
        AUDDBG("Could not read received data from last.fm. What's up?\n");
        return FALSE;
    }

    char *status = check_status(error_code, error_detail);

    if (!status) {
        AUDDBG("Status was NULL. Invalid API answer.\n");
        clean_data();
        return FALSE;
    }

    if (!strcmp(status, "failed")) {
        result = FALSE;

    } else {
        str_unref(username);
        username = get_attribute_value("/lfm/recommendations[@user]", "user");
        if (!username) {
          AUDDBG("last.fm not answering according to the API.\n");
          result = FALSE;
        }
    }

    str_unref(status);

    clean_data();
    return result;
}



bool_t read_token (char **error_code, char **error_detail) {

    *error_code = NULL;
    *error_detail = NULL;

    bool_t result = TRUE;

    if (!prepare_data()) {
        AUDDBG("Could not read received data from last.fm. What's up?\n");
        return FALSE;
    }

    char *status = check_status(error_code, error_detail);

    if (!status) {
        AUDDBG("Status was NULL. Invalid API answer.\n");
        clean_data();
        return FALSE;
    }

    if (!strcmp(status, "failed")) {
        AUDDBG("Error code: %s. Detail: %s.\n", *error_code, *error_detail);
        result = FALSE;
    }
    else {
        str_unref(request_token);
        request_token = get_node_string("/lfm/token");

        if (!request_token || !request_token[0]) {
            AUDDBG("Could not read the received token. Something's wrong with the API?\n");
            result = FALSE;
        }
        else {
            AUDDBG("This is the token: %s.\nNice? Nice.\n", request_token);
        }
    }

    str_unref(status);

    clean_data();
    return result;
}



bool_t read_session_key(char **error_code, char **error_detail) {

    *error_code = NULL;
    *error_detail = NULL;

    bool_t result = TRUE;

    if (!prepare_data()) {
        AUDDBG("Could not read received data from last.fm. What's up?\n");
        return FALSE;
    }

    char *status = check_status(error_code, error_detail);

    if (!status) {
        AUDDBG("Status was NULL or empty. Invalid API answer.\n");
        clean_data();
        return FALSE;
    }

    if (!strcmp(status, "failed")) {
        AUDDBG("Error code: %s. Detail: %s.\n", *error_code, *error_detail);
        result = FALSE;

    } else {
        str_unref(session_key);
        session_key = get_node_string("/lfm/session/key");

        if (!session_key || !session_key[0]) {
            AUDDBG("Could not read the received session key. Something's wrong with the API?\n");
            result = FALSE;
        } else {
            AUDDBG("This is the session key: %s.\n", session_key);
        }
    }

    str_unref(status);

    clean_data();
    return result;
}

