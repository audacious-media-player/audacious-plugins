/*
 *  Wget like SSL certificate validation from environmental variables.
 *  Copyright (C) 2009 Jussi Judin
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <ne_ssl.h>
#include <ne_session.h>

#include <libaudcore/audstrings.h>

#include "cert_verification.h"

// Certificate validation handling.

const int LENGTH_SHORTFORM_MAX = 127;
const int LENGTH_INDEFINITE_VALUE = 128;
const int LENGTH_LONGFORM_MASK = 0x7f;
const int OCTET_BITS = 8;
const int ASNTYPE_CODE_MASK = 0x1f;
const int HASH_BYTES = 4;
const uint32_t MAX_CERT_CHECKS = UINT32_MAX;

enum AsnType
{
    ASNTYPE_INTEGER = 2,
    ASNTYPE_SEQUENCE = 16,
    ASNTYPE_VARLEN_IDENTIFIER = 31
};

struct DerData
{
    unsigned char * start;
    unsigned char * end;
    unsigned char * nextStart;
    unsigned char * bufferEnd;
    enum AsnType type;
};

/**
 * X.509 certificate has following structure defined in RFC5280:
 *
 * Certificate  ::= SEQUENCE  {
 *      tbsCertificate       TBSCertificate,
 *      signatureAlgorithm   AlgorithmIdentifier,
 *      signatureValue       BIT STRING  }
 *
 * TBSCertificate  ::=  SEQUENCE  {
 *      version         [0]  EXPLICIT Version DEFAULT v1,
 *      serialNumber         CertificateSerialNumber,
 *      signature            AlgorithmIdentifier,
 *      issuer               Name,
 *      validity             Validity,
 *      subject              Name,
 * ...
 *
 * Version  ::=  INTEGER  {  v1(0), v2(1), v3(2)  }
 * CertificateSerialNumber  ::=  INTEGER
 * AlgorithmIdentifier  ::=  SEQUENCE  {
 *      algorithm               OBJECT IDENTIFIER,
 *      parameters              ANY DEFINED BY algorithm OPTIONAL  }
 * Validity ::= SEQUENCE {
 *      notBefore      Time,
 *      notAfter       Time }
 * Time ::= CHOICE {
 *      utcTime        UTCTime,
 *      generalTime    GeneralizedTime }
 * Name ::= CHOICE { -- only one possibility for now --
 *   rdnSequence  RDNSequence }
 * RDNSequence ::= SEQUENCE OF RelativeDistinguishedName
 *
 * And we are only interested in subject field.
 */

/**
 * Returns the tag number of DER data.
 */
static bool der_read_tag_number (unsigned char * in_buffer,
 const unsigned char * in_bufferEnd, unsigned char * * out_lengthStart,
 enum AsnType * out_type)
{
    if (in_bufferEnd - in_buffer < 2)
        return false;

    unsigned char typeOctet = in_buffer[0];
    enum AsnType typeCode = (AsnType) (typeOctet & ASNTYPE_CODE_MASK);
    int typeCodeLength = 1;

    // Variable length types are not supported.
    if (typeCode == ASNTYPE_VARLEN_IDENTIFIER)
        return false;

    * out_type = typeCode;
    * out_lengthStart = in_buffer + typeCodeLength;

    return true;
}

/**
 * Returns pointers that point to the content of DER data value when identifier
 * is not present.
 */
static bool der_read_content_length (unsigned char * in_buffer,
 const unsigned char * in_bufferEnd, unsigned char * * out_start,
 unsigned char * * out_end, unsigned char * * out_nextStart)
{
    if (in_bufferEnd - in_buffer < 1)
        return false;

    unsigned char lengthDescription = in_buffer[0];

    if (lengthDescription <= LENGTH_SHORTFORM_MAX)
    {
        * out_start = in_buffer + 1;
        * out_end = * out_start + lengthDescription;

        if (in_bufferEnd < * out_end)
            return false;

        * out_nextStart = * out_end;
        return true;
    }

    if (lengthDescription == LENGTH_INDEFINITE_VALUE)
    {
        * out_start = in_buffer + 1;
        unsigned char * currentPos = in_buffer + 1;

        while (currentPos < in_bufferEnd - 1)
        {
            if (! currentPos[0] && ! currentPos[1])
            {
                * out_end = currentPos;
                * out_nextStart = * out_end + 1;
                return true;
            }
        }

        return false;
    }

    size_t lengthOctets = lengthDescription & LENGTH_LONGFORM_MASK;

    // We can't handle longer objects than size_t can represent.
    if (lengthOctets > sizeof (size_t))
        return false;

    if (in_bufferEnd < in_buffer + 1 + lengthOctets)
        return false;

    size_t contentLength = 0;
    size_t i = 0;

    for (i = 0; i < lengthOctets; i++)
    {
        contentLength <<= OCTET_BITS;
        contentLength |= in_buffer[i + 1];
    }

    * out_start = in_buffer + 1 + lengthOctets;
    * out_end = * out_start + contentLength;

    if (in_bufferEnd < * out_end)
        return false;

    * out_nextStart = * out_end;
    return true;
}

/**
 * Returns pointers that point to the content of DER data value.
 */
static bool der_read_content (struct DerData * data, struct DerData * content)
{
    unsigned char * lengthStart = nullptr;
    bool typeOk = der_read_tag_number (data->start, data->bufferEnd,
     & lengthStart, & content->type);

    if (! typeOk)
        return false;

    content->bufferEnd = data->bufferEnd;
    return der_read_content_length (lengthStart, data->bufferEnd,
     & content->start, & content->end, & content->nextStart);
}

/**
 * Returns pointers that point to next DER data value.
 */
static bool der_read_next (struct DerData * currentContent, struct DerData * nextContent)
{
    nextContent->start = currentContent->nextStart;

    unsigned char * lengthStart = nullptr;
    bool typeOk = der_read_tag_number (currentContent->start,
     currentContent->bufferEnd, & lengthStart, & nextContent->type);

    if (! typeOk)
        return false;

    unsigned char * nextContentStart = nullptr;
    return der_read_content_length (lengthStart, currentContent->bufferEnd,
     & nextContentStart, & nextContent->end, & nextContent->nextStart);
}

/**
 * Returns certificate hash that can be used to certificate links generated
 * by c_rehash command.
 *
 * Certificate hash is just 4 first octets of MD5 sum of certificate subject id
 * DER format.
 *
 * @return TRUE if given certificate could be parsed by this, FALSE otherwise.
 */
static bool cert_get_hash (const ne_ssl_certificate * cert, uint32_t * out_hash)
{
    char * certPem = ne_ssl_cert_export (cert);
    g_return_val_if_fail (certPem != nullptr, 1);
    size_t derLength = 0;
    unsigned char * certDer = g_base64_decode (certPem, &derLength);
    g_free (certPem);
    g_return_val_if_fail (certDer != nullptr, 1);

    struct DerData data = {0};
    struct DerData content = {0};

    data.start = certDer;
    data.bufferEnd = certDer + derLength;

    // Walk through certificate content until we reach subject field.

    // certificate
    g_return_val_if_fail (der_read_content (& data, & content), false);
    g_return_val_if_fail (ASNTYPE_SEQUENCE == content.type, false);
    // tbsCertificate
    g_return_val_if_fail (der_read_content (& content, & content), false);
    g_return_val_if_fail (ASNTYPE_SEQUENCE == content.type, false);
    // version + serialNumber
    g_return_val_if_fail (der_read_content (& content, & content), false);
    g_return_val_if_fail (ASNTYPE_INTEGER == content.type, false);
    // signature
    g_return_val_if_fail (der_read_next (& content, & content), false);
    g_return_val_if_fail (ASNTYPE_SEQUENCE == content.type, false);
    // issuer
    g_return_val_if_fail (der_read_next (& content, & content), false);
    g_return_val_if_fail (ASNTYPE_SEQUENCE == content.type, false);
    // validity
    g_return_val_if_fail (der_read_next (& content, & content), false);
    g_return_val_if_fail (ASNTYPE_SEQUENCE == content.type, false);
    // subject
    g_return_val_if_fail (der_read_next (& content, & content), false);
    g_return_val_if_fail (ASNTYPE_SEQUENCE == content.type, false);

    // Calculate MD5 sum of subject.
    unsigned char md5pword[16];
    size_t md5len = 16;

    GChecksum * state = g_checksum_new (G_CHECKSUM_MD5);
    g_checksum_update (state, content.start, content.nextStart - content.start);
    g_checksum_get_digest (state, md5pword, & md5len);
    g_checksum_free (state);

    uint32_t hash = 0;

    // Hash is reverse of four first bytes of MD5 checksum of DER encoded
    // subject ASN.1 field.
    for (int i = HASH_BYTES - 1; i >= 0; i --)
    {
        hash <<= OCTET_BITS;
        hash |= md5pword[i];
    }

    * out_hash = hash;

    g_free (certDer);
    return true;
}

/**
 * Checks if given certificate is signed by given signer certificate.
 *
 * Goes up in certificate signer chain of certificate and compares if any one
 * of them is same as given signer.
 */
static bool is_signer_of_cert (const ne_ssl_certificate * signer, const ne_ssl_certificate * cert)
{
    const ne_ssl_certificate * certSigner = cert;

    while (certSigner != nullptr)
    {
        if (ne_ssl_cert_cmp (signer, certSigner) == 0)
            return true;

        certSigner = ne_ssl_cert_signedby (certSigner);
    }

    return false;
}

/**
 * Checks if given file includes certificate that has signed given certificate.
 */
static bool file_is_signer_of_cert (const char * filename, const ne_ssl_certificate * cert)
{
    ne_ssl_certificate * signer = ne_ssl_cert_read (filename);

    if (signer != nullptr)
    {
        bool signOk = is_signer_of_cert (signer, cert);
        ne_ssl_cert_free (signer);

        if (signOk)
            return true;
    }

    return false;
}

/**
 * Checks if directory includes a file that can be signer of given certificate.
 */
static bool validate_directory_certs (const char * directory,
 const ne_ssl_certificate * serverCert, uint32_t certHash)
{
    // Search certificate names in ascending order and assume that all names
    // point to valid certificates.
    uint32_t certId = 0;

    while (certId < MAX_CERT_CHECKS)
    {
        // Construct certificate name.
        StringBuf certFilename = str_printf ("%08x.%d", certHash, certId);
        char * certPath = g_build_filename (directory, (const char *) certFilename, nullptr);

        bool signOk = file_is_signer_of_cert (certPath, serverCert);
        g_free (certPath);

        if (signOk)
            return true;

        certId ++;
    }

    return false;
}

/**
 * Checks if given certificate is valid certificate according to certificates
 * found from SSL_CERT_FILE or SSL_CERT_DIR environmental variables.
 */
int neon_vfs_verify_environment_ssl_certs (void * userdata, int failures,
 const ne_ssl_certificate * serverCert)
{
    // First check the certificate file, if we have one.
    const char * sslCertFile = g_getenv ("SSL_CERT_FILE");

    if (sslCertFile != nullptr)
    {
        if (file_is_signer_of_cert (sslCertFile, serverCert))
            return failures & ~NE_SSL_UNTRUSTED;
    }

    // check if we have list of directories where certificates can be.
    const char * sslCertDirPaths = g_getenv ("SSL_CERT_DIR");

    if (sslCertDirPaths == nullptr)
        return failures;

    uint32_t certHash = 0;
    g_return_val_if_fail (cert_get_hash (serverCert, & certHash), failures);

    char * sslCertDirPathsStart = g_strdup (sslCertDirPaths);
    char * sslCertDirPathsEnd = sslCertDirPathsStart + strlen (sslCertDirPathsStart);
    char * sslCertDir = sslCertDirPathsStart;
    char * dirnameStart = sslCertDir;
    char * dirnameEnd = dirnameStart;

    // Start going through all directories in SSL_CERT_DIR
    for (; dirnameEnd <= sslCertDirPathsEnd; dirnameEnd ++)
    {
        if (* dirnameEnd == G_SEARCHPATH_SEPARATOR || dirnameEnd == sslCertDirPathsEnd)
        {
            * dirnameEnd = 0;

            // Skip empty directories
            if (dirnameStart[0] == 0)
            {
                // Start next directory name after the inserted zero.
                dirnameStart = dirnameEnd + 1;
                continue;
            }

            if (validate_directory_certs (dirnameStart, serverCert, certHash))
            {
                g_free (sslCertDirPathsStart);
                return failures & ~NE_SSL_UNTRUSTED;
            }

            // Start next directory name after the inserted zero.
            dirnameStart = dirnameEnd + 1;
        }
    }

    g_free (sslCertDirPathsStart);

    return failures;
}
