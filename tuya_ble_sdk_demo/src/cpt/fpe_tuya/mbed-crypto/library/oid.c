/**
 * \file oid.c
 *
 * \brief Object Identifier (OID) database
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of Mbed Crypto (https://tls.mbed.org)
 */

#if !defined(MBEDCRYPTO_CONFIG_FILE)
#include "mbedcrypto/config.h"
#else
#include MBEDCRYPTO_CONFIG_FILE
#endif

#if defined(MBEDCRYPTO_OID_C)

#include "mbedcrypto/oid.h"
#include "mbedcrypto/rsa.h"

#include <stdio.h>
#include <string.h>

#if defined(MBEDCRYPTO_PLATFORM_C)
#include "mbedcrypto/platform.h"
#else
#define mbedcrypto_snprintf snprintf
#endif

#if defined(MBEDCRYPTO_X509_USE_C) || defined(MBEDCRYPTO_X509_CREATE_C)
#include "mbedcrypto/x509.h"
#endif

/*
 * Macro to automatically add the size of #define'd OIDs
 */
#define ADD_LEN(s)      s, MBEDCRYPTO_OID_SIZE(s)

/*
 * Macro to generate an internal function for oid_XXX_from_asn1() (used by
 * the other functions)
 */
#define FN_OID_TYPED_FROM_ASN1( TYPE_T, NAME, LIST )                        \
static const TYPE_T * oid_ ## NAME ## _from_asn1( const mbedcrypto_asn1_buf *oid )     \
{                                                                           \
    const TYPE_T *p = LIST;                                                 \
    const mbedcrypto_oid_descriptor_t *cur = (const mbedcrypto_oid_descriptor_t *) p;             \
    if( p == NULL || oid == NULL ) return( NULL );                          \
    while( cur->asn1 != NULL ) {                                            \
        if( cur->asn1_len == oid->len &&                                    \
            memcmp( cur->asn1, oid->p, oid->len ) == 0 ) {                  \
            return( p );                                                    \
        }                                                                   \
        p++;                                                                \
        cur = (const mbedcrypto_oid_descriptor_t *) p;                                 \
    }                                                                       \
    return( NULL );                                                         \
}

/*
 * Macro to generate a function for retrieving a single attribute from the
 * descriptor of an mbedcrypto_oid_descriptor_t wrapper.
 */
#define FN_OID_GET_DESCRIPTOR_ATTR1(FN_NAME, TYPE_T, TYPE_NAME, ATTR1_TYPE, ATTR1) \
int FN_NAME( const mbedcrypto_asn1_buf *oid, ATTR1_TYPE * ATTR1 )                  \
{                                                                       \
    const TYPE_T *data = oid_ ## TYPE_NAME ## _from_asn1( oid );        \
    if( data == NULL ) return( MBEDCRYPTO_ERR_OID_NOT_FOUND );            \
    *ATTR1 = data->descriptor.ATTR1;                                    \
    return( 0 );                                                        \
}

/*
 * Macro to generate a function for retrieving a single attribute from an
 * mbedcrypto_oid_descriptor_t wrapper.
 */
#define FN_OID_GET_ATTR1(FN_NAME, TYPE_T, TYPE_NAME, ATTR1_TYPE, ATTR1) \
int FN_NAME( const mbedcrypto_asn1_buf *oid, ATTR1_TYPE * ATTR1 )                  \
{                                                                       \
    const TYPE_T *data = oid_ ## TYPE_NAME ## _from_asn1( oid );        \
    if( data == NULL ) return( MBEDCRYPTO_ERR_OID_NOT_FOUND );            \
    *ATTR1 = data->ATTR1;                                               \
    return( 0 );                                                        \
}

/*
 * Macro to generate a function for retrieving two attributes from an
 * mbedcrypto_oid_descriptor_t wrapper.
 */
#define FN_OID_GET_ATTR2(FN_NAME, TYPE_T, TYPE_NAME, ATTR1_TYPE, ATTR1,     \
                         ATTR2_TYPE, ATTR2)                                 \
int FN_NAME( const mbedcrypto_asn1_buf *oid, ATTR1_TYPE * ATTR1, ATTR2_TYPE * ATTR2 )  \
{                                                                           \
    const TYPE_T *data = oid_ ## TYPE_NAME ## _from_asn1( oid );            \
    if( data == NULL ) return( MBEDCRYPTO_ERR_OID_NOT_FOUND );                \
    *ATTR1 = data->ATTR1;                                                   \
    *ATTR2 = data->ATTR2;                                                   \
    return( 0 );                                                            \
}

/*
 * Macro to generate a function for retrieving the OID based on a single
 * attribute from a mbedcrypto_oid_descriptor_t wrapper.
 */
#define FN_OID_GET_OID_BY_ATTR1(FN_NAME, TYPE_T, LIST, ATTR1_TYPE, ATTR1)   \
int FN_NAME( ATTR1_TYPE ATTR1, const char **oid, size_t *olen )             \
{                                                                           \
    const TYPE_T *cur = LIST;                                               \
    while( cur->descriptor.asn1 != NULL ) {                                 \
        if( cur->ATTR1 == ATTR1 ) {                                         \
            *oid = cur->descriptor.asn1;                                    \
            *olen = cur->descriptor.asn1_len;                               \
            return( 0 );                                                    \
        }                                                                   \
        cur++;                                                              \
    }                                                                       \
    return( MBEDCRYPTO_ERR_OID_NOT_FOUND );                                   \
}

/*
 * Macro to generate a function for retrieving the OID based on two
 * attributes from a mbedcrypto_oid_descriptor_t wrapper.
 */
#define FN_OID_GET_OID_BY_ATTR2(FN_NAME, TYPE_T, LIST, ATTR1_TYPE, ATTR1,   \
                                ATTR2_TYPE, ATTR2)                          \
int FN_NAME( ATTR1_TYPE ATTR1, ATTR2_TYPE ATTR2, const char **oid ,         \
             size_t *olen )                                                 \
{                                                                           \
    const TYPE_T *cur = LIST;                                               \
    while( cur->descriptor.asn1 != NULL ) {                                 \
        if( cur->ATTR1 == ATTR1 && cur->ATTR2 == ATTR2 ) {                  \
            *oid = cur->descriptor.asn1;                                    \
            *olen = cur->descriptor.asn1_len;                               \
            return( 0 );                                                    \
        }                                                                   \
        cur++;                                                              \
    }                                                                       \
    return( MBEDCRYPTO_ERR_OID_NOT_FOUND );                                   \
}

#if defined(MBEDCRYPTO_X509_USE_C) || defined(MBEDCRYPTO_X509_CREATE_C)
/*
 * For X520 attribute types
 */
typedef struct {
    mbedcrypto_oid_descriptor_t    descriptor;
    const char          *short_name;
} oid_x520_attr_t;

static const oid_x520_attr_t oid_x520_attr_type[] =
{
    {
        { ADD_LEN( MBEDCRYPTO_OID_AT_CN ),          "id-at-commonName",               "Common Name" },
        "CN",
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_AT_COUNTRY ),     "id-at-countryName",              "Country" },
        "C",
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_AT_LOCALITY ),    "id-at-locality",                 "Locality" },
        "L",
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_AT_STATE ),       "id-at-state",                    "State" },
        "ST",
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_AT_ORGANIZATION ),"id-at-organizationName",         "Organization" },
        "O",
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_AT_ORG_UNIT ),    "id-at-organizationalUnitName",   "Org Unit" },
        "OU",
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_PKCS9_EMAIL ),    "emailAddress",                   "E-mail address" },
        "emailAddress",
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_AT_SERIAL_NUMBER ),"id-at-serialNumber",            "Serial number" },
        "serialNumber",
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_AT_POSTAL_ADDRESS ),"id-at-postalAddress",          "Postal address" },
        "postalAddress",
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_AT_POSTAL_CODE ), "id-at-postalCode",               "Postal code" },
        "postalCode",
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_AT_SUR_NAME ),    "id-at-surName",                  "Surname" },
        "SN",
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_AT_GIVEN_NAME ),  "id-at-givenName",                "Given name" },
        "GN",
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_AT_INITIALS ),    "id-at-initials",                 "Initials" },
        "initials",
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_AT_GENERATION_QUALIFIER ), "id-at-generationQualifier", "Generation qualifier" },
        "generationQualifier",
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_AT_TITLE ),       "id-at-title",                    "Title" },
        "title",
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_AT_DN_QUALIFIER ),"id-at-dnQualifier",              "Distinguished Name qualifier" },
        "dnQualifier",
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_AT_PSEUDONYM ),   "id-at-pseudonym",                "Pseudonym" },
        "pseudonym",
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_DOMAIN_COMPONENT ), "id-domainComponent",           "Domain component" },
        "DC",
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_AT_UNIQUE_IDENTIFIER ), "id-at-uniqueIdentifier",    "Unique Identifier" },
        "uniqueIdentifier",
    },
    {
        { NULL, 0, NULL, NULL },
        NULL,
    }
};

FN_OID_TYPED_FROM_ASN1(oid_x520_attr_t, x520_attr, oid_x520_attr_type)
FN_OID_GET_ATTR1(mbedcrypto_oid_get_attr_short_name, oid_x520_attr_t, x520_attr, const char *, short_name)

/*
 * For X509 extensions
 */
typedef struct {
    mbedcrypto_oid_descriptor_t    descriptor;
    int                 ext_type;
} oid_x509_ext_t;

static const oid_x509_ext_t oid_x509_ext[] =
{
    {
        { ADD_LEN( MBEDCRYPTO_OID_BASIC_CONSTRAINTS ),    "id-ce-basicConstraints",   "Basic Constraints" },
        MBEDCRYPTO_X509_EXT_BASIC_CONSTRAINTS,
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_KEY_USAGE ),            "id-ce-keyUsage",           "Key Usage" },
        MBEDCRYPTO_X509_EXT_KEY_USAGE,
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_EXTENDED_KEY_USAGE ),   "id-ce-extKeyUsage",        "Extended Key Usage" },
        MBEDCRYPTO_X509_EXT_EXTENDED_KEY_USAGE,
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_SUBJECT_ALT_NAME ),     "id-ce-subjectAltName",     "Subject Alt Name" },
        MBEDCRYPTO_X509_EXT_SUBJECT_ALT_NAME,
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_NS_CERT_TYPE ),         "id-netscape-certtype",     "Netscape Certificate Type" },
        MBEDCRYPTO_X509_EXT_NS_CERT_TYPE,
    },
    {
        { NULL, 0, NULL, NULL },
        0,
    },
};

FN_OID_TYPED_FROM_ASN1(oid_x509_ext_t, x509_ext, oid_x509_ext)
FN_OID_GET_ATTR1(mbedcrypto_oid_get_x509_ext_type, oid_x509_ext_t, x509_ext, int, ext_type)

static const mbedcrypto_oid_descriptor_t oid_ext_key_usage[] =
{
    { ADD_LEN( MBEDCRYPTO_OID_SERVER_AUTH ),      "id-kp-serverAuth",      "TLS Web Server Authentication" },
    { ADD_LEN( MBEDCRYPTO_OID_CLIENT_AUTH ),      "id-kp-clientAuth",      "TLS Web Client Authentication" },
    { ADD_LEN( MBEDCRYPTO_OID_CODE_SIGNING ),     "id-kp-codeSigning",     "Code Signing" },
    { ADD_LEN( MBEDCRYPTO_OID_EMAIL_PROTECTION ), "id-kp-emailProtection", "E-mail Protection" },
    { ADD_LEN( MBEDCRYPTO_OID_TIME_STAMPING ),    "id-kp-timeStamping",    "Time Stamping" },
    { ADD_LEN( MBEDCRYPTO_OID_OCSP_SIGNING ),     "id-kp-OCSPSigning",     "OCSP Signing" },
    { NULL, 0, NULL, NULL },
};

FN_OID_TYPED_FROM_ASN1(mbedcrypto_oid_descriptor_t, ext_key_usage, oid_ext_key_usage)
FN_OID_GET_ATTR1(mbedcrypto_oid_get_extended_key_usage, mbedcrypto_oid_descriptor_t, ext_key_usage, const char *, description)
#endif /* MBEDCRYPTO_X509_USE_C || MBEDCRYPTO_X509_CREATE_C */

#if defined(MBEDCRYPTO_MD_C)
/*
 * For SignatureAlgorithmIdentifier
 */
typedef struct {
    mbedcrypto_oid_descriptor_t    descriptor;
    mbedcrypto_md_type_t           md_alg;
    mbedcrypto_pk_type_t           pk_alg;
} oid_sig_alg_t;

static const oid_sig_alg_t oid_sig_alg[] =
{
#if defined(MBEDCRYPTO_RSA_C)
#if defined(MBEDCRYPTO_MD2_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_PKCS1_MD2 ),        "md2WithRSAEncryption",     "RSA with MD2" },
        MBEDCRYPTO_MD_MD2,      MBEDCRYPTO_PK_RSA,
    },
#endif /* MBEDCRYPTO_MD2_C */
#if defined(MBEDCRYPTO_MD4_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_PKCS1_MD4 ),        "md4WithRSAEncryption",     "RSA with MD4" },
        MBEDCRYPTO_MD_MD4,      MBEDCRYPTO_PK_RSA,
    },
#endif /* MBEDCRYPTO_MD4_C */
#if defined(MBEDCRYPTO_MD5_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_PKCS1_MD5 ),        "md5WithRSAEncryption",     "RSA with MD5" },
        MBEDCRYPTO_MD_MD5,      MBEDCRYPTO_PK_RSA,
    },
#endif /* MBEDCRYPTO_MD5_C */
#if defined(MBEDCRYPTO_SHA1_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_PKCS1_SHA1 ),       "sha-1WithRSAEncryption",   "RSA with SHA1" },
        MBEDCRYPTO_MD_SHA1,     MBEDCRYPTO_PK_RSA,
    },
#endif /* MBEDCRYPTO_SHA1_C */
#if defined(MBEDCRYPTO_SHA256_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_PKCS1_SHA224 ),     "sha224WithRSAEncryption",  "RSA with SHA-224" },
        MBEDCRYPTO_MD_SHA224,   MBEDCRYPTO_PK_RSA,
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_PKCS1_SHA256 ),     "sha256WithRSAEncryption",  "RSA with SHA-256" },
        MBEDCRYPTO_MD_SHA256,   MBEDCRYPTO_PK_RSA,
    },
#endif /* MBEDCRYPTO_SHA256_C */
#if defined(MBEDCRYPTO_SHA512_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_PKCS1_SHA384 ),     "sha384WithRSAEncryption",  "RSA with SHA-384" },
        MBEDCRYPTO_MD_SHA384,   MBEDCRYPTO_PK_RSA,
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_PKCS1_SHA512 ),     "sha512WithRSAEncryption",  "RSA with SHA-512" },
        MBEDCRYPTO_MD_SHA512,   MBEDCRYPTO_PK_RSA,
    },
#endif /* MBEDCRYPTO_SHA512_C */
#if defined(MBEDCRYPTO_SHA1_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_RSA_SHA_OBS ),      "sha-1WithRSAEncryption",   "RSA with SHA1" },
        MBEDCRYPTO_MD_SHA1,     MBEDCRYPTO_PK_RSA,
    },
#endif /* MBEDCRYPTO_SHA1_C */
#endif /* MBEDCRYPTO_RSA_C */
#if defined(MBEDCRYPTO_ECDSA_C)
#if defined(MBEDCRYPTO_SHA1_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_ECDSA_SHA1 ),       "ecdsa-with-SHA1",      "ECDSA with SHA1" },
        MBEDCRYPTO_MD_SHA1,     MBEDCRYPTO_PK_ECDSA,
    },
#endif /* MBEDCRYPTO_SHA1_C */
#if defined(MBEDCRYPTO_SHA256_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_ECDSA_SHA224 ),     "ecdsa-with-SHA224",    "ECDSA with SHA224" },
        MBEDCRYPTO_MD_SHA224,   MBEDCRYPTO_PK_ECDSA,
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_ECDSA_SHA256 ),     "ecdsa-with-SHA256",    "ECDSA with SHA256" },
        MBEDCRYPTO_MD_SHA256,   MBEDCRYPTO_PK_ECDSA,
    },
#endif /* MBEDCRYPTO_SHA256_C */
#if defined(MBEDCRYPTO_SHA512_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_ECDSA_SHA384 ),     "ecdsa-with-SHA384",    "ECDSA with SHA384" },
        MBEDCRYPTO_MD_SHA384,   MBEDCRYPTO_PK_ECDSA,
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_ECDSA_SHA512 ),     "ecdsa-with-SHA512",    "ECDSA with SHA512" },
        MBEDCRYPTO_MD_SHA512,   MBEDCRYPTO_PK_ECDSA,
    },
#endif /* MBEDCRYPTO_SHA512_C */
#endif /* MBEDCRYPTO_ECDSA_C */
#if defined(MBEDCRYPTO_RSA_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_RSASSA_PSS ),        "RSASSA-PSS",           "RSASSA-PSS" },
        MBEDCRYPTO_MD_NONE,     MBEDCRYPTO_PK_RSASSA_PSS,
    },
#endif /* MBEDCRYPTO_RSA_C */
    {
        { NULL, 0, NULL, NULL },
        MBEDCRYPTO_MD_NONE, MBEDCRYPTO_PK_NONE,
    },
};

FN_OID_TYPED_FROM_ASN1(oid_sig_alg_t, sig_alg, oid_sig_alg)
FN_OID_GET_DESCRIPTOR_ATTR1(mbedcrypto_oid_get_sig_alg_desc, oid_sig_alg_t, sig_alg, const char *, description)
FN_OID_GET_ATTR2(mbedcrypto_oid_get_sig_alg, oid_sig_alg_t, sig_alg, mbedcrypto_md_type_t, md_alg, mbedcrypto_pk_type_t, pk_alg)
FN_OID_GET_OID_BY_ATTR2(mbedcrypto_oid_get_oid_by_sig_alg, oid_sig_alg_t, oid_sig_alg, mbedcrypto_pk_type_t, pk_alg, mbedcrypto_md_type_t, md_alg)
#endif /* MBEDCRYPTO_MD_C */

/*
 * For PublicKeyInfo (PKCS1, RFC 5480)
 */
typedef struct {
    mbedcrypto_oid_descriptor_t    descriptor;
    mbedcrypto_pk_type_t           pk_alg;
} oid_pk_alg_t;

static const oid_pk_alg_t oid_pk_alg[] =
{
    {
        { ADD_LEN( MBEDCRYPTO_OID_PKCS1_RSA ),      "rsaEncryption",   "RSA" },
        MBEDCRYPTO_PK_RSA,
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_EC_ALG_UNRESTRICTED ),  "id-ecPublicKey",   "Generic EC key" },
        MBEDCRYPTO_PK_ECKEY,
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_EC_ALG_ECDH ),          "id-ecDH",          "EC key for ECDH" },
        MBEDCRYPTO_PK_ECKEY_DH,
    },
    {
        { NULL, 0, NULL, NULL },
        MBEDCRYPTO_PK_NONE,
    },
};

FN_OID_TYPED_FROM_ASN1(oid_pk_alg_t, pk_alg, oid_pk_alg)
FN_OID_GET_ATTR1(mbedcrypto_oid_get_pk_alg, oid_pk_alg_t, pk_alg, mbedcrypto_pk_type_t, pk_alg)
FN_OID_GET_OID_BY_ATTR1(mbedcrypto_oid_get_oid_by_pk_alg, oid_pk_alg_t, oid_pk_alg, mbedcrypto_pk_type_t, pk_alg)

#if defined(MBEDCRYPTO_ECP_C)
/*
 * For namedCurve (RFC 5480)
 */
typedef struct {
    mbedcrypto_oid_descriptor_t    descriptor;
    mbedcrypto_ecp_group_id        grp_id;
} oid_ecp_grp_t;

static const oid_ecp_grp_t oid_ecp_grp[] =
{
#if defined(MBEDCRYPTO_ECP_DP_SECP192R1_ENABLED)
    {
        { ADD_LEN( MBEDCRYPTO_OID_EC_GRP_SECP192R1 ), "secp192r1",    "secp192r1" },
        MBEDCRYPTO_ECP_DP_SECP192R1,
    },
#endif /* MBEDCRYPTO_ECP_DP_SECP192R1_ENABLED */
#if defined(MBEDCRYPTO_ECP_DP_SECP224R1_ENABLED)
    {
        { ADD_LEN( MBEDCRYPTO_OID_EC_GRP_SECP224R1 ), "secp224r1",    "secp224r1" },
        MBEDCRYPTO_ECP_DP_SECP224R1,
    },
#endif /* MBEDCRYPTO_ECP_DP_SECP224R1_ENABLED */
#if defined(MBEDCRYPTO_ECP_DP_SECP256R1_ENABLED)
    {
        { ADD_LEN( MBEDCRYPTO_OID_EC_GRP_SECP256R1 ), "secp256r1",    "secp256r1" },
        MBEDCRYPTO_ECP_DP_SECP256R1,
    },
#endif /* MBEDCRYPTO_ECP_DP_SECP256R1_ENABLED */
#if defined(MBEDCRYPTO_ECP_DP_SECP384R1_ENABLED)
    {
        { ADD_LEN( MBEDCRYPTO_OID_EC_GRP_SECP384R1 ), "secp384r1",    "secp384r1" },
        MBEDCRYPTO_ECP_DP_SECP384R1,
    },
#endif /* MBEDCRYPTO_ECP_DP_SECP384R1_ENABLED */
#if defined(MBEDCRYPTO_ECP_DP_SECP521R1_ENABLED)
    {
        { ADD_LEN( MBEDCRYPTO_OID_EC_GRP_SECP521R1 ), "secp521r1",    "secp521r1" },
        MBEDCRYPTO_ECP_DP_SECP521R1,
    },
#endif /* MBEDCRYPTO_ECP_DP_SECP521R1_ENABLED */
#if defined(MBEDCRYPTO_ECP_DP_SECP192K1_ENABLED)
    {
        { ADD_LEN( MBEDCRYPTO_OID_EC_GRP_SECP192K1 ), "secp192k1",    "secp192k1" },
        MBEDCRYPTO_ECP_DP_SECP192K1,
    },
#endif /* MBEDCRYPTO_ECP_DP_SECP192K1_ENABLED */
#if defined(MBEDCRYPTO_ECP_DP_SECP224K1_ENABLED)
    {
        { ADD_LEN( MBEDCRYPTO_OID_EC_GRP_SECP224K1 ), "secp224k1",    "secp224k1" },
        MBEDCRYPTO_ECP_DP_SECP224K1,
    },
#endif /* MBEDCRYPTO_ECP_DP_SECP224K1_ENABLED */
#if defined(MBEDCRYPTO_ECP_DP_SECP256K1_ENABLED)
    {
        { ADD_LEN( MBEDCRYPTO_OID_EC_GRP_SECP256K1 ), "secp256k1",    "secp256k1" },
        MBEDCRYPTO_ECP_DP_SECP256K1,
    },
#endif /* MBEDCRYPTO_ECP_DP_SECP256K1_ENABLED */
#if defined(MBEDCRYPTO_ECP_DP_BP256R1_ENABLED)
    {
        { ADD_LEN( MBEDCRYPTO_OID_EC_GRP_BP256R1 ),   "brainpoolP256r1","brainpool256r1" },
        MBEDCRYPTO_ECP_DP_BP256R1,
    },
#endif /* MBEDCRYPTO_ECP_DP_BP256R1_ENABLED */
#if defined(MBEDCRYPTO_ECP_DP_BP384R1_ENABLED)
    {
        { ADD_LEN( MBEDCRYPTO_OID_EC_GRP_BP384R1 ),   "brainpoolP384r1","brainpool384r1" },
        MBEDCRYPTO_ECP_DP_BP384R1,
    },
#endif /* MBEDCRYPTO_ECP_DP_BP384R1_ENABLED */
#if defined(MBEDCRYPTO_ECP_DP_BP512R1_ENABLED)
    {
        { ADD_LEN( MBEDCRYPTO_OID_EC_GRP_BP512R1 ),   "brainpoolP512r1","brainpool512r1" },
        MBEDCRYPTO_ECP_DP_BP512R1,
    },
#endif /* MBEDCRYPTO_ECP_DP_BP512R1_ENABLED */
    {
        { NULL, 0, NULL, NULL },
        MBEDCRYPTO_ECP_DP_NONE,
    },
};

FN_OID_TYPED_FROM_ASN1(oid_ecp_grp_t, grp_id, oid_ecp_grp)
FN_OID_GET_ATTR1(mbedcrypto_oid_get_ec_grp, oid_ecp_grp_t, grp_id, mbedcrypto_ecp_group_id, grp_id)
FN_OID_GET_OID_BY_ATTR1(mbedcrypto_oid_get_oid_by_ec_grp, oid_ecp_grp_t, oid_ecp_grp, mbedcrypto_ecp_group_id, grp_id)
#endif /* MBEDCRYPTO_ECP_C */

#if defined(MBEDCRYPTO_CIPHER_C)
/*
 * For PKCS#5 PBES2 encryption algorithm
 */
typedef struct {
    mbedcrypto_oid_descriptor_t    descriptor;
    mbedcrypto_cipher_type_t       cipher_alg;
} oid_cipher_alg_t;

static const oid_cipher_alg_t oid_cipher_alg[] =
{
    {
        { ADD_LEN( MBEDCRYPTO_OID_DES_CBC ),              "desCBC",       "DES-CBC" },
        MBEDCRYPTO_CIPHER_DES_CBC,
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_DES_EDE3_CBC ),         "des-ede3-cbc", "DES-EDE3-CBC" },
        MBEDCRYPTO_CIPHER_DES_EDE3_CBC,
    },
    {
        { NULL, 0, NULL, NULL },
        MBEDCRYPTO_CIPHER_NONE,
    },
};

FN_OID_TYPED_FROM_ASN1(oid_cipher_alg_t, cipher_alg, oid_cipher_alg)
FN_OID_GET_ATTR1(mbedcrypto_oid_get_cipher_alg, oid_cipher_alg_t, cipher_alg, mbedcrypto_cipher_type_t, cipher_alg)
#endif /* MBEDCRYPTO_CIPHER_C */

#if defined(MBEDCRYPTO_MD_C)
/*
 * For digestAlgorithm
 */
typedef struct {
    mbedcrypto_oid_descriptor_t    descriptor;
    mbedcrypto_md_type_t           md_alg;
} oid_md_alg_t;

static const oid_md_alg_t oid_md_alg[] =
{
#if defined(MBEDCRYPTO_MD2_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_DIGEST_ALG_MD2 ),       "id-md2",       "MD2" },
        MBEDCRYPTO_MD_MD2,
    },
#endif /* MBEDCRYPTO_MD2_C */
#if defined(MBEDCRYPTO_MD4_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_DIGEST_ALG_MD4 ),       "id-md4",       "MD4" },
        MBEDCRYPTO_MD_MD4,
    },
#endif /* MBEDCRYPTO_MD4_C */
#if defined(MBEDCRYPTO_MD5_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_DIGEST_ALG_MD5 ),       "id-md5",       "MD5" },
        MBEDCRYPTO_MD_MD5,
    },
#endif /* MBEDCRYPTO_MD5_C */
#if defined(MBEDCRYPTO_SHA1_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_DIGEST_ALG_SHA1 ),      "id-sha1",      "SHA-1" },
        MBEDCRYPTO_MD_SHA1,
    },
#endif /* MBEDCRYPTO_SHA1_C */
#if defined(MBEDCRYPTO_SHA256_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_DIGEST_ALG_SHA224 ),    "id-sha224",    "SHA-224" },
        MBEDCRYPTO_MD_SHA224,
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_DIGEST_ALG_SHA256 ),    "id-sha256",    "SHA-256" },
        MBEDCRYPTO_MD_SHA256,
    },
#endif /* MBEDCRYPTO_SHA256_C */
#if defined(MBEDCRYPTO_SHA512_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_DIGEST_ALG_SHA384 ),    "id-sha384",    "SHA-384" },
        MBEDCRYPTO_MD_SHA384,
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_DIGEST_ALG_SHA512 ),    "id-sha512",    "SHA-512" },
        MBEDCRYPTO_MD_SHA512,
    },
#endif /* MBEDCRYPTO_SHA512_C */
    {
        { NULL, 0, NULL, NULL },
        MBEDCRYPTO_MD_NONE,
    },
};

FN_OID_TYPED_FROM_ASN1(oid_md_alg_t, md_alg, oid_md_alg)
FN_OID_GET_ATTR1(mbedcrypto_oid_get_md_alg, oid_md_alg_t, md_alg, mbedcrypto_md_type_t, md_alg)
FN_OID_GET_OID_BY_ATTR1(mbedcrypto_oid_get_oid_by_md, oid_md_alg_t, oid_md_alg, mbedcrypto_md_type_t, md_alg)

/*
 * For HMAC digestAlgorithm
 */
typedef struct {
    mbedcrypto_oid_descriptor_t    descriptor;
    mbedcrypto_md_type_t           md_hmac;
} oid_md_hmac_t;

static const oid_md_hmac_t oid_md_hmac[] =
{
#if defined(MBEDCRYPTO_SHA1_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_HMAC_SHA1 ),      "hmacSHA1",      "HMAC-SHA-1" },
        MBEDCRYPTO_MD_SHA1,
    },
#endif /* MBEDCRYPTO_SHA1_C */
#if defined(MBEDCRYPTO_SHA256_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_HMAC_SHA224 ),    "hmacSHA224",    "HMAC-SHA-224" },
        MBEDCRYPTO_MD_SHA224,
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_HMAC_SHA256 ),    "hmacSHA256",    "HMAC-SHA-256" },
        MBEDCRYPTO_MD_SHA256,
    },
#endif /* MBEDCRYPTO_SHA256_C */
#if defined(MBEDCRYPTO_SHA512_C)
    {
        { ADD_LEN( MBEDCRYPTO_OID_HMAC_SHA384 ),    "hmacSHA384",    "HMAC-SHA-384" },
        MBEDCRYPTO_MD_SHA384,
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_HMAC_SHA512 ),    "hmacSHA512",    "HMAC-SHA-512" },
        MBEDCRYPTO_MD_SHA512,
    },
#endif /* MBEDCRYPTO_SHA512_C */
    {
        { NULL, 0, NULL, NULL },
        MBEDCRYPTO_MD_NONE,
    },
};

FN_OID_TYPED_FROM_ASN1(oid_md_hmac_t, md_hmac, oid_md_hmac)
FN_OID_GET_ATTR1(mbedcrypto_oid_get_md_hmac, oid_md_hmac_t, md_hmac, mbedcrypto_md_type_t, md_hmac)
#endif /* MBEDCRYPTO_MD_C */

#if defined(MBEDCRYPTO_PKCS12_C)
/*
 * For PKCS#12 PBEs
 */
typedef struct {
    mbedcrypto_oid_descriptor_t    descriptor;
    mbedcrypto_md_type_t           md_alg;
    mbedcrypto_cipher_type_t       cipher_alg;
} oid_pkcs12_pbe_alg_t;

static const oid_pkcs12_pbe_alg_t oid_pkcs12_pbe_alg[] =
{
    {
        { ADD_LEN( MBEDCRYPTO_OID_PKCS12_PBE_SHA1_DES3_EDE_CBC ), "pbeWithSHAAnd3-KeyTripleDES-CBC", "PBE with SHA1 and 3-Key 3DES" },
        MBEDCRYPTO_MD_SHA1,      MBEDCRYPTO_CIPHER_DES_EDE3_CBC,
    },
    {
        { ADD_LEN( MBEDCRYPTO_OID_PKCS12_PBE_SHA1_DES2_EDE_CBC ), "pbeWithSHAAnd2-KeyTripleDES-CBC", "PBE with SHA1 and 2-Key 3DES" },
        MBEDCRYPTO_MD_SHA1,      MBEDCRYPTO_CIPHER_DES_EDE_CBC,
    },
    {
        { NULL, 0, NULL, NULL },
        MBEDCRYPTO_MD_NONE, MBEDCRYPTO_CIPHER_NONE,
    },
};

FN_OID_TYPED_FROM_ASN1(oid_pkcs12_pbe_alg_t, pkcs12_pbe_alg, oid_pkcs12_pbe_alg)
FN_OID_GET_ATTR2(mbedcrypto_oid_get_pkcs12_pbe_alg, oid_pkcs12_pbe_alg_t, pkcs12_pbe_alg, mbedcrypto_md_type_t, md_alg, mbedcrypto_cipher_type_t, cipher_alg)
#endif /* MBEDCRYPTO_PKCS12_C */

#define OID_SAFE_SNPRINTF                               \
    do {                                                \
        if( ret < 0 || (size_t) ret >= n )              \
            return( MBEDCRYPTO_ERR_OID_BUF_TOO_SMALL );    \
                                                        \
        n -= (size_t) ret;                              \
        p += (size_t) ret;                              \
    } while( 0 )

/* Return the x.y.z.... style numeric string for the given OID */
int mbedcrypto_oid_get_numeric_string( char *buf, size_t size,
                            const mbedcrypto_asn1_buf *oid )
{
    int ret;
    size_t i, n;
    unsigned int value;
    char *p;

    p = buf;
    n = size;

    /* First byte contains first two dots */
    if( oid->len > 0 )
    {
        ret = mbedcrypto_snprintf( p, n, "%d.%d", oid->p[0] / 40, oid->p[0] % 40 );
        OID_SAFE_SNPRINTF;
    }

    value = 0;
    for( i = 1; i < oid->len; i++ )
    {
        /* Prevent overflow in value. */
        if( ( ( value << 7 ) >> 7 ) != value )
            return( MBEDCRYPTO_ERR_OID_BUF_TOO_SMALL );

        value <<= 7;
        value += oid->p[i] & 0x7F;

        if( !( oid->p[i] & 0x80 ) )
        {
            /* Last byte */
            ret = mbedcrypto_snprintf( p, n, ".%d", value );
            OID_SAFE_SNPRINTF;
            value = 0;
        }
    }

    return( (int) ( size - n ) );
}

#endif /* MBEDCRYPTO_OID_C */