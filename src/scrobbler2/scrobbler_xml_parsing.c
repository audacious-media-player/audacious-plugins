
//plugin includes
#include "scrobbler.h"


//audacious includes

static xmlDocPtr doc = NULL;
static xmlXPathContextPtr context = NULL;

static bool_t prepare_data () {
    received_data[received_data_size] = '\0';
    printf("Data received from last.fm:\n%s\n%%%%End of data%%%%\n", received_data);

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
static xmlChar *get_attribute_value (xmlChar *node_expression, xmlChar *attribute) {
    if (doc == NULL || context == NULL) {
        AUDDBG("Response from last.fm not parsed successfully. Did you call prepare_data?\n");
        return NULL;
    }

    xmlXPathObjectPtr statusObj;
    xmlChar *result;


    statusObj = xmlXPathEvalExpression((xmlChar *) node_expression, context);
    if (statusObj == NULL) {
        AUDDBG ("Error in xmlXPathEvalExpression.\n");
        return NULL;
    }
    if (xmlXPathNodeSetIsEmpty(statusObj->nodesetval)) {
        AUDDBG("No result.\n");
        xmlXPathFreeObject(statusObj);
        return NULL;
    }

    //TODO: DELETE THIS FOR. THIS IS JUST FOR DEBUGGING
    for (int i = 0 ; i < statusObj->nodesetval->nodeNr ; i++) {
        AUDDBG("Children found for expression (ATTRIBUTE [%s] text find):\n%s\n", node_expression,
                xmlGetProp(statusObj->nodesetval->nodeTab[i], (xmlChar *) attribute));
    }

    result = xmlGetProp(statusObj->nodesetval->nodeTab[0], (xmlChar *) attribute);

    xmlXPathFreeObject(statusObj);
    AUDDBG("RESULT FOR THIS FUNCTION: %s.\n", result);
    return result;
}

//returns:
// NULL if an error occurs or the node was not found
static xmlChar *get_node_string (const char * node_expression) {
    if (doc == NULL || context == NULL) {
        AUDDBG("Response from last.fm not parsed successfully. Did you call prepare_data?\n");
        return NULL;
    }

    xmlXPathObjectPtr statusObj;
    xmlChar *result;


    statusObj = xmlXPathEvalExpression((xmlChar *) node_expression, context);
    if (statusObj == NULL) {
        AUDDBG ("Error in xmlXPathEvalExpression.\n");
        return NULL;
    }
    if (xmlXPathNodeSetIsEmpty(statusObj->nodesetval)) {
        AUDDBG("No result.\n");
        xmlXPathFreeObject(statusObj);
        return NULL;
    }

    //TODO: DELETE THIS FOR. THIS IS JUST FOR DEBUGGING
    for (int i = 0 ; i < statusObj->nodesetval->nodeNr ; i++) {
        AUDDBG("Children found for expression (NODE [%s] text find):\n%s\n", node_expression,
                xmlNodeListGetString(doc, statusObj->nodesetval->nodeTab[i]->children, 1));
    }

    result = xmlNodeListGetString(doc, statusObj->nodesetval->nodeTab[0]->children, 1);
    xmlXPathFreeObject(statusObj);
    AUDDBG("RESULT FOR THIS FUNCTION: %s.\n", result);
    return result;
}


//returns:
// NULL if an error occurs
// "true" if the the command succeeded
// "false" if an error occurred. error_code and error_detail should be checked in this case
static xmlChar *check_status (xmlChar **error_code, xmlChar **error_detail) {
    (*error_code) = NULL;
    (*error_detail) = NULL;

    xmlChar *status = get_attribute_value((xmlChar *) "/lfm[@status]", (xmlChar *) "status");
    if (status == NULL) {
        AUDDBG("last.fm not answering according to the API.\n");
        return NULL;
    }

    AUDDBG ("status is %s.\n", status);
    if (!xmlStrEqual(status, (xmlChar *) "ok")) {

        (*error_code) = get_attribute_value((xmlChar *) "/lfm/error[@code]", (xmlChar *) "code");
        if ((*error_code) == NULL) {
            AUDDBG("Weird API answer. Last.fm says status is %s but there is no error code?\n", status);
            xmlFree(status);
            status = NULL;
        } else {
            (*error_detail) = get_node_string("/lfm/error");
        }
    }

    AUDDBG("check_status results: return: %s. error_code: %s. error_detail: %s.\n", status, (*error_code), (*error_detail));
    return status;
}

bool_t read_scrobble_result(char **error_code_out, char **error_detail_out) {
    xmlChar *status;
    xmlChar *error_code = NULL;
    xmlChar *error_detail = NULL;
    bool_t result = TRUE;


    if (!prepare_data()) {
        AUDDBG("Could not read received data from last.fm. What's up?\n");
        return FALSE;
    }

    status = check_status(&error_code, &error_detail);

    (*error_code_out) = g_strdup((gchar *) error_code);
    (*error_detail_out) = g_strdup((gchar *) error_detail);


    if (status == NULL) {
        AUDDBG("Status was NULL. Invalid API answer.\n");
        clean_data();
        return FALSE;
    }

    if (xmlStrEqual(status, (xmlChar *) "failed")) {
        AUDDBG("Error code: %s. Detail: %s.\n", error_code, error_detail);
        result = FALSE;
    }


    xmlFree(status);
    if (error_code != NULL) {
        xmlFree(error_code);
    }
    if (error_detail != NULL) {
        xmlFree(error_detail);
    }

    clean_data();
    return result;
}

//returns
//FALSE if there was an error with the connection
bool_t read_connection_test_result () {
    xmlChar *status;
    xmlChar *error_code = NULL;
    xmlChar *error_detail = NULL;
    bool_t result = TRUE;


    if (!prepare_data()) {
        AUDDBG("Could not read received data from last.fm. What's up?\n");
        return FALSE;
    }

    status = check_status(&error_code, &error_detail);
    if (status == NULL) {
        AUDDBG("Status was NULL. Invalid API answer.\n");
        clean_data();
        return FALSE;
    }

    if (xmlStrEqual(status, (xmlChar *) "failed")) {
        AUDDBG("Error code: %s. Detail: %s.\n", error_code, error_detail);
        if (xmlStrEqual(error_code, (xmlChar *) "4")) {
            //error code 4: Authentication Failed - You do not have permissions to access the service
            result = FALSE;
        } else if (xmlStrEqual(error_code, (xmlChar *) "9")) {
            //error code 9: Invalid session key - Please re-authenticate
            result = FALSE;
        } else {
            //all other errors are irrelevant for authentication detection
            result = TRUE;
        }
    }

    xmlFree(status);
    if (error_code != NULL) {
        xmlFree(error_code);
    }
    if (error_detail != NULL) {
        xmlFree(error_detail);
    }

    clean_data();
    return result;
}



bool_t read_token () {
    xmlChar *status;
    xmlChar *error_code = NULL;
    xmlChar *error_detail = NULL;
    bool_t result = TRUE;


    if (!prepare_data()) {
        AUDDBG("Could not read received data from last.fm. What's up?\n");
        return FALSE;
    }

    status = check_status(&error_code, &error_detail);
    if (status == NULL) {
        AUDDBG("Status was NULL. Invalid API answer.\n");
        clean_data();
        return FALSE;
    }

    if (xmlStrEqual(status, (xmlChar *) "failed")) {
        AUDDBG("Error code: %s. Detail: %s.\n", error_code, error_detail);
        //TODO: react accordingly
        result = FALSE;
    }
    else {
        request_token = (gchar *) get_node_string("/lfm/token");

        if (request_token == NULL) {
            AUDDBG("Could not read the received token. Something's wrong with the API?\n");
            result = FALSE;
        }
        else {
            AUDDBG("This is the token: %s.\nNice? Nice.\n", request_token);
        }
    }

    xmlFree(status);
    if (error_code != NULL) {
        xmlFree(error_code);
    }
    if (error_detail != NULL) {
        xmlFree(error_detail);
    }

    clean_data();
    return result;
}



bool_t read_session_key(char **error_code_out, char **error_detail_out) {
    xmlChar *status;
    xmlChar *error_code = NULL;
    xmlChar *error_detail = NULL;
    bool_t result = TRUE;


    if (!prepare_data()) {
        AUDDBG("Could not read received data from last.fm. What's up?\n");
        return FALSE;
    }

    status = check_status(&error_code, &error_detail);
    (*error_code_out) = g_strdup((gchar *) error_code);
    (*error_detail_out) = g_strdup((gchar *) error_detail);

    if (status == NULL) {
        AUDDBG("Status was NULL. Invalid API answer.\n");
        clean_data();
        return FALSE;
    }

    if (xmlStrEqual(status, (xmlChar *) "failed")) {
        AUDDBG("Error code: %s. Detail: %s.\n", error_code, error_detail);
        //TODO: react accordingly
        result = FALSE;
    }
    else {
        session_key = (gchar *) get_node_string("/lfm/session/key");

        if (session_key == NULL) {
            AUDDBG("Could not read the received session key. Something's wrong with the API?\n");
            result = FALSE;
        }
        else {
            AUDDBG("This is the session key: %s.\n", session_key);
        }
    }

    xmlFree(status);
    if (error_code != NULL) {
        xmlFree(error_code);
    }
    if (error_detail != NULL) {
        xmlFree(error_detail);
    }

    clean_data();
    return result;
}

