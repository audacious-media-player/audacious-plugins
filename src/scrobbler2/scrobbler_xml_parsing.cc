
//plugin includes
#include "scrobbler.h"

//static (private) variables
static xmlDocPtr doc = nullptr;
static xmlXPathContextPtr context = nullptr;

static gboolean prepare_data () {
    received_data[received_data_size] = '\0';
    AUDDBG("Data received from last.fm:\n%s\n%%%%End of data%%%%\n", received_data);

    doc = xmlParseMemory(received_data, received_data_size+1);
    received_data_size = 0;
    if (doc == nullptr) {
        AUDDBG("Document not parsed successfully.\n");
        return false;
    }

    context = xmlXPathNewContext(doc);
    if (context == nullptr) {
        AUDDBG("Error in xmlXPathNewContext\n");
        xmlFreeDoc(doc);
        doc = nullptr;
        return false;
    }
    return true;
}

static void clean_data() {
    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);
    context = nullptr;
    doc = nullptr;
}


//returns:
// nullptr if an error occurs or the attribute was not found
// the attribute's value if it was found
static String get_attribute_value (const char *node_expression, const char *attribute) {
    if (doc == nullptr || context == nullptr) {
        AUDDBG("Response from last.fm not parsed successfully. Did you call prepare_data?\n");
        return String();
    }

    xmlXPathObjectPtr statusObj = xmlXPathEvalExpression((xmlChar *) node_expression, context);
    if (statusObj == nullptr) {
        AUDDBG ("Error in xmlXPathEvalExpression.\n");
        return String();
    }
    if (xmlXPathNodeSetIsEmpty(statusObj->nodesetval)) {
        AUDDBG("No result.\n");
        xmlXPathFreeObject(statusObj);
        return String();
    }

    xmlChar *prop = xmlGetProp(statusObj->nodesetval->nodeTab[0], (xmlChar *) attribute);

    String result;
    if (prop && prop[0])
        result = String((const char *)prop);

    xmlXPathFreeObject(statusObj);
    xmlFree(prop);

    AUDDBG("RESULT FOR THIS FUNCTION: %s.\n", (const char *)result);
    return result;
}

//returns:
// nullptr if an error occurs or the node was not found
static String get_node_string (const char *node_expression) {
    if (doc == nullptr || context == nullptr) {
        AUDDBG("Response from last.fm not parsed successfully. Did you call prepare_data?\n");
        return String();
    }

    xmlXPathObjectPtr statusObj = xmlXPathEvalExpression((xmlChar *) node_expression, context);
    if (statusObj == nullptr) {
        AUDDBG ("Error in xmlXPathEvalExpression.\n");
        return String();
    }
    if (xmlXPathNodeSetIsEmpty(statusObj->nodesetval)) {
        AUDDBG("No result.\n");
        xmlXPathFreeObject(statusObj);
        return String();
    }

    xmlChar *string = xmlNodeListGetString(doc, statusObj->nodesetval->nodeTab[0]->children, 1);

    String result;
    if (string && string[0])
        result = String((const char *) string);

    xmlXPathFreeObject(statusObj);
    xmlFree(string);

    AUDDBG("RESULT FOR THIS FUNCTION: %s.\n", (const char *)result);
    return result;
}


//returns:
// nullptr if an error occurs
// "true" if the the command succeeded
// "false" if an error occurred. error_code and error_detail should be checked in this case
static String check_status (String &error_code, String &error_detail) {

    String status = get_attribute_value("/lfm[@status]", "status");
    if (!status) {
        AUDDBG("last.fm not answering according to the API.\n");
        return String();
    }

    AUDDBG ("status is %s.\n", (const char *)status);
    if (strcmp(status, "ok")) {

        error_code = get_attribute_value("/lfm/error[@code]", "code");
        if (!(*error_code)) {
            AUDDBG("Weird API answer. Last.fm says status is %s but there is no error code?\n",
             (const char *)status);
            status = String();
        } else {
            error_detail = get_node_string("/lfm/error");
        }
    }

    AUDDBG("check_status results: return: %s. error_code: %s. error_detail: %s.\n",
     (const char *)status, (const char *)error_code, (const char *)error_detail);
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
 *      * They are nullptr if an API communication error occur
 */
gboolean read_scrobble_result(String &error_code, String &error_detail,
 gboolean *ignored, String &ignored_code) {

    *ignored = false;

    gboolean result = true;

    if (!prepare_data()) {
        AUDDBG("Could not read received data from last.fm. What's up?\n");
        return false;
    }

    String status = check_status(error_code, error_detail);

    if (!status) {
        AUDDBG("Status was nullptr. Invalid API answer.\n");
        clean_data();
        return false;
    }

    if (!strcmp(status, "failed")) {
        AUDDBG("Error code: %s. Detail: %s.\n", (const char *)error_code,
         (const char *)error_detail);
        result = false;

    } else {
        //TODO: We are assuming that only one track is scrobbled per request! This will have to be
        //re-done to support multiple tracks being scrobbled in batch
        String ignored_scrobble = get_attribute_value("/lfm/scrobbles[@ignored]", "ignored");

        if (ignored_scrobble && strcmp(ignored_scrobble, "0")) {
          //The track was ignored
          //TODO: this assumes ignored_scrobble == 1!!!
          *ignored = true;
          ignored_code = get_attribute_value("/lfm/scrobbles/scrobble/ignoredMessage[@code]", "code");
        }

        AUDDBG("ignored? %i, ignored_code: %s\n", *ignored, (const char *)ignored_code);
    }

    clean_data();
    return result;
}

//returns
//FALSE if there was an error with the connection
gboolean read_authentication_test_result (String &error_code, String &error_detail) {

    gboolean result = true;

    if (!prepare_data()) {
        AUDDBG("Could not read received data from last.fm. What's up?\n");
        return false;
    }

    String status = check_status(error_code, error_detail);

    if (!status) {
        AUDDBG("Status was nullptr. Invalid API answer.\n");
        clean_data();
        return false;
    }

    if (!strcmp(status, "failed")) {
        result = false;

    } else {
        username = get_node_string("/lfm/user/name");
        if (!username) {
          AUDERR("last.fm not answering according to the API.\n");
          result = false;
        }
    }

    clean_data();
    return result;
}



gboolean read_token (String &error_code, String &error_detail) {

    gboolean result = true;

    if (!prepare_data()) {
        AUDDBG("Could not read received data from last.fm. What's up?\n");
        return false;
    }

    String status = check_status(error_code, error_detail);

    if (!status) {
        AUDDBG("Status was nullptr. Invalid API answer.\n");
        clean_data();
        return false;
    }

    if (!strcmp(status, "failed")) {
        AUDDBG("Error code: %s. Detail: %s.\n", (const char *)error_code,
         (const char *)error_detail);
        result = false;
    }
    else {
        request_token = get_node_string("/lfm/token");

        if (!request_token || !request_token[0]) {
            AUDDBG("Could not read the received token. Something's wrong with the API?\n");
            result = false;
        }
        else {
            AUDDBG("This is the token: %s.\nNice? Nice.\n", (const char *)request_token);
        }
    }

    clean_data();
    return result;
}



gboolean read_session_key(String &error_code, String &error_detail) {

    gboolean result = true;

    if (!prepare_data()) {
        AUDDBG("Could not read received data from last.fm. What's up?\n");
        return false;
    }

    String status = check_status(error_code, error_detail);

    if (!status) {
        AUDDBG("Status was nullptr or empty. Invalid API answer.\n");
        clean_data();
        return false;
    }

    if (!strcmp(status, "failed")) {
        AUDDBG("Error code: %s. Detail: %s.\n", (const char *)error_code,
         (const char *)error_detail);
        result = false;

    } else {
        session_key = get_node_string("/lfm/session/key");

        if (!session_key || !session_key[0]) {
            AUDDBG("Could not read the received session key. Something's wrong with the API?\n");
            result = false;
        } else {
            AUDDBG("This is the session key: %s.\n", (const char *)session_key);
        }
    }

    clean_data();
    return result;
}

