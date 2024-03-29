# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ssl_srcs = [
    "ssl/bio_ssl.cc",
    "ssl/custom_extensions.cc",
    "ssl/d1_both.cc",
    "ssl/d1_lib.cc",
    "ssl/d1_pkt.cc",
    "ssl/d1_srtp.cc",
    "ssl/dtls_method.cc",
    "ssl/dtls_record.cc",
    "ssl/encrypted_client_hello.cc",
    "ssl/extensions.cc",
    "ssl/handoff.cc",
    "ssl/handshake.cc",
    "ssl/handshake_client.cc",
    "ssl/handshake_server.cc",
    "ssl/s3_both.cc",
    "ssl/s3_lib.cc",
    "ssl/s3_pkt.cc",
    "ssl/ssl_aead_ctx.cc",
    "ssl/ssl_asn1.cc",
    "ssl/ssl_buffer.cc",
    "ssl/ssl_cert.cc",
    "ssl/ssl_cipher.cc",
    "ssl/ssl_decrepit.c",
    "ssl/ssl_file.cc",
    "ssl/ssl_key_share.cc",
    "ssl/ssl_lib.cc",
    "ssl/ssl_privkey.cc",
    "ssl/ssl_session.cc",
    "ssl/ssl_stat.cc",
    "ssl/ssl_transcript.cc",
    "ssl/ssl_transfer_asn1.cc",
    "ssl/ssl_versions.cc",
    "ssl/ssl_x509.cc",
    "ssl/t1_enc.cc",
    "ssl/tls13_both.cc",
    "ssl/tls13_client.cc",
    "ssl/tls13_enc.cc",
    "ssl/tls13_server.cc",
    "ssl/tls_method.cc",
    "ssl/tls_record.cc",
]

ssl_hdrs = [
    "include/openssl/dtls1.h",
    "include/openssl/ssl.h",
    "include/openssl/ssl3.h",
    "include/openssl/tls1.h",
]

fips_fragments = [
    "crypto/fipsmodule/aes/aes.c",
    "crypto/fipsmodule/aes/aes_nohw.c",
    "crypto/fipsmodule/aes/key_wrap.c",
    "crypto/fipsmodule/aes/mode_wrappers.c",
    "crypto/fipsmodule/bn/add.c",
    "crypto/fipsmodule/bn/asm/x86_64-gcc.c",
    "crypto/fipsmodule/bn/bn.c",
    "crypto/fipsmodule/bn/bytes.c",
    "crypto/fipsmodule/bn/cmp.c",
    "crypto/fipsmodule/bn/ctx.c",
    "crypto/fipsmodule/bn/div.c",
    "crypto/fipsmodule/bn/div_extra.c",
    "crypto/fipsmodule/bn/exponentiation.c",
    "crypto/fipsmodule/bn/gcd.c",
    "crypto/fipsmodule/bn/gcd_extra.c",
    "crypto/fipsmodule/bn/generic.c",
    "crypto/fipsmodule/bn/jacobi.c",
    "crypto/fipsmodule/bn/montgomery.c",
    "crypto/fipsmodule/bn/montgomery_inv.c",
    "crypto/fipsmodule/bn/mul.c",
    "crypto/fipsmodule/bn/prime.c",
    "crypto/fipsmodule/bn/random.c",
    "crypto/fipsmodule/bn/rsaz_exp.c",
    "crypto/fipsmodule/bn/shift.c",
    "crypto/fipsmodule/bn/sqrt.c",
    "crypto/fipsmodule/cipher/aead.c",
    "crypto/fipsmodule/cipher/cipher.c",
    "crypto/fipsmodule/cipher/e_aes.c",
    "crypto/fipsmodule/cipher/e_aesccm.c",
    "crypto/fipsmodule/cmac/cmac.c",
    "crypto/fipsmodule/cpucap/cpu_aarch64_apple.c",
    "crypto/fipsmodule/cpucap/cpu_aarch64_freebsd.c",
    "crypto/fipsmodule/cpucap/cpu_aarch64_fuchsia.c",
    "crypto/fipsmodule/cpucap/cpu_aarch64_linux.c",
    "crypto/fipsmodule/cpucap/cpu_aarch64_openbsd.c",
    "crypto/fipsmodule/cpucap/cpu_aarch64_win.c",
    "crypto/fipsmodule/cpucap/cpu_ppc64le.c",
    "crypto/fipsmodule/cpucap/cpu_arm_freebsd.c",
    "crypto/fipsmodule/cpucap/cpu_arm_linux.c",
    "crypto/fipsmodule/cpucap/cpu_intel.c",
    "crypto/fipsmodule/dh/check.c",
    "crypto/fipsmodule/dh/dh.c",
    "crypto/fipsmodule/digest/digest.c",
    "crypto/fipsmodule/digest/digests.c",
    "crypto/fipsmodule/ec/ec.c",
    "crypto/fipsmodule/ec/ec_key.c",
    "crypto/fipsmodule/ec/ec_montgomery.c",
    "crypto/fipsmodule/ec/felem.c",
    "crypto/fipsmodule/ec/oct.c",
    "crypto/fipsmodule/ec/p224-64.c",
    "crypto/fipsmodule/ec/p256-nistz.c",
    "crypto/fipsmodule/ec/p256.c",
    "crypto/fipsmodule/ec/p384.c",
    "crypto/fipsmodule/ec/p521.c",
    "crypto/fipsmodule/ec/scalar.c",
    "crypto/fipsmodule/ec/simple.c",
    "crypto/fipsmodule/ec/simple_mul.c",
    "crypto/fipsmodule/ec/util.c",
    "crypto/fipsmodule/ec/wnaf.c",
    "crypto/fipsmodule/ecdh/ecdh.c",
    "crypto/fipsmodule/ecdsa/ecdsa.c",
    "crypto/fipsmodule/evp/digestsign.c",
    "crypto/fipsmodule/evp/evp.c",
    "crypto/fipsmodule/evp/evp_ctx.c",
    "crypto/fipsmodule/evp/p_ec.c",
    "crypto/fipsmodule/evp/p_hkdf.c",
    "crypto/fipsmodule/evp/p_rsa.c",
    "crypto/fipsmodule/pbkdf/pbkdf.c",
    "crypto/fipsmodule/hkdf/hkdf.c",
    "crypto/fipsmodule/hmac/hmac.c",
    "crypto/fipsmodule/md4/md4.c",
    "crypto/fipsmodule/md5/md5.c",
    "crypto/fipsmodule/modes/cbc.c",
    "crypto/fipsmodule/modes/cfb.c",
    "crypto/fipsmodule/modes/ctr.c",
    "crypto/fipsmodule/modes/gcm.c",
    "crypto/fipsmodule/modes/gcm_nohw.c",
    "crypto/fipsmodule/modes/ofb.c",
    "crypto/fipsmodule/modes/polyval.c",
    "crypto/fipsmodule/modes/xts.c",
    "crypto/fipsmodule/rand/ctrdrbg.c",
    "crypto/fipsmodule/rand/fork_detect.c",
    "crypto/fipsmodule/rand/rand.c",
    "crypto/fipsmodule/rand/urandom.c",
    "crypto/fipsmodule/rsa/blinding.c",
    "crypto/fipsmodule/rsa/padding.c",
    "crypto/fipsmodule/rsa/rsa.c",
    "crypto/fipsmodule/rsa/rsa_impl.c",
    "crypto/fipsmodule/self_check/fips.c",
    "crypto/fipsmodule/self_check/self_check.c",
    "crypto/fipsmodule/service_indicator/service_indicator.c",
    "crypto/fipsmodule/sha/keccak1600.c",
    "crypto/fipsmodule/sha/sha1.c",
    "crypto/fipsmodule/sha/sha1-altivec.c",
    "crypto/fipsmodule/sha/sha256.c",
    "crypto/fipsmodule/sha/sha3.c",
    "crypto/fipsmodule/sha/sha512.c",
    "crypto/fipsmodule/sshkdf/sshkdf.c",
    "crypto/fipsmodule/tls/kdf.c",
    "crypto/kyber/pqcrystals_kyber_ref_common/cbd.c",
    "crypto/kyber/pqcrystals_kyber_ref_common/indcpa.c",
    "crypto/kyber/pqcrystals_kyber_ref_common/kem.c",
    "crypto/kyber/pqcrystals_kyber_ref_common/ntt.c",
    "crypto/kyber/pqcrystals_kyber_ref_common/poly.c",
    "crypto/kyber/pqcrystals_kyber_ref_common/polyvec.c",
    "crypto/kyber/pqcrystals_kyber_ref_common/reduce.c",
    "crypto/kyber/pqcrystals_kyber_ref_common/symmetric-shake.c",
    "crypto/kyber/pqcrystals_kyber_ref_common/verify.c",
]

ssl_internal_hdrs = [
    "ssl/internal.h",
]

crypto_hdrs = [
    "include/openssl/aead.h",
    "include/openssl/aes.h",
    "include/openssl/arm_arch.h",
    "include/openssl/asn1.h",
    "include/openssl/asn1_mac.h",
    "include/openssl/asn1t.h",
    "include/openssl/base.h",
    "include/openssl/base64.h",
    "include/openssl/bio.h",
    "include/openssl/blake2.h",
    "include/openssl/blowfish.h",
    "include/openssl/bn.h",
    "include/openssl/buf.h",
    "include/openssl/buffer.h",
    "include/openssl/bytestring.h",
    "include/openssl/chacha.h",
    "include/openssl/cipher.h",
    "include/openssl/cmac.h",
    "include/openssl/conf.h",
    "include/openssl/cpu.h",
    "include/openssl/crypto.h",
    "include/openssl/ctrdrbg.h",
    "include/openssl/curve25519.h",
    "include/openssl/des.h",
    "include/openssl/dh.h",
    "include/openssl/digest.h",
    "include/openssl/is_awslc.h",
    "include/openssl/dsa.h",
    "include/openssl/e_os2.h",
    "include/openssl/ec.h",
    "include/openssl/ec_key.h",
    "include/openssl/ecdh.h",
    "include/openssl/ecdsa.h",
    "include/openssl/engine.h",
    "include/openssl/err.h",
    "include/openssl/evp.h",
    "include/openssl/evp_errors.h",
    "include/openssl/ex_data.h",
    "include/openssl/hkdf.h",
    "include/openssl/hmac.h",
    "include/openssl/hpke.h",
    "include/openssl/hrss.h",
    "include/openssl/kdf.h",
    "include/openssl/lhash.h",
    "include/openssl/md4.h",
    "include/openssl/md5.h",
    "include/openssl/mem.h",
    "include/openssl/nid.h",
    "include/openssl/obj.h",
    "include/openssl/obj_mac.h",
    "include/openssl/objects.h",
    #
    "include/openssl/ocsp.h",
    "include/openssl/opensslconf.h",
    "include/openssl/opensslv.h",
    "include/openssl/ossl_typ.h",
    "include/openssl/pem.h",
    "include/openssl/pkcs12.h",
    "include/openssl/pkcs7.h",
    "include/openssl/pkcs8.h",
    "include/openssl/poly1305.h",
    "include/openssl/pool.h",
    "include/openssl/rand.h",
    "include/openssl/rc4.h",
    "include/openssl/ripemd.h",
    "include/openssl/rsa.h",
    "include/openssl/safestack.h",
    "include/openssl/service_indicator.h",
    "include/openssl/sha.h",
    "include/openssl/siphash.h",
    "include/openssl/span.h",
    "include/openssl/sshkdf.h",
    "include/openssl/stack.h",
    "include/openssl/thread.h",
    "include/openssl/trust_token.h",
    "include/openssl/type_check.h",
    "include/openssl/x509.h",
    "include/openssl/x509_vfy.h",
    "include/openssl/x509v3.h",
]

crypto_internal_hdrs = [
    "crypto/asn1/internal.h",
    "crypto/bio/internal.h",
    "crypto/bytestring/internal.h",
    "crypto/chacha/internal.h",
    "crypto/cipher_extra/internal.h",
    "crypto/conf/conf_def.h",
    "crypto/conf/internal.h",
    "crypto/kem/internal.h",
    "crypto/kyber/kem_kyber.h",
    "crypto/kyber/pqcrystals_kyber_ref_common/api.h",
    "crypto/kyber/pqcrystals_kyber_ref_common/cbd.h",
    "crypto/kyber/pqcrystals_kyber_ref_common/fips202.h",
    "crypto/kyber/pqcrystals_kyber_ref_common/indcpa.h",
    "crypto/kyber/pqcrystals_kyber_ref_common/kem.h",
    "crypto/kyber/pqcrystals_kyber_ref_common/ntt.h",
    "crypto/kyber/pqcrystals_kyber_ref_common/params.h",
    "crypto/kyber/pqcrystals_kyber_ref_common/poly.h",
    "crypto/kyber/pqcrystals_kyber_ref_common/polyvec.h",
    "crypto/kyber/pqcrystals_kyber_ref_common/reduce.h",
    "crypto/kyber/pqcrystals_kyber_ref_common/symmetric.h",
    "crypto/kyber/pqcrystals_kyber_ref_common/verify.h",
    "crypto/rand_extra/pq_custom_randombytes.h",
    "crypto/ocsp/internal.h",
    "crypto/rsa_extra/internal.h",
    "crypto/curve25519/curve25519_tables.h",
    "crypto/curve25519/internal.h",
    "crypto/des/internal.h",
    "crypto/dsa/internal.h",
    "crypto/ec_extra/internal.h",
    "crypto/evp_extra/internal.h",
    "crypto/err/internal.h",
    "crypto/fipsmodule/aes/internal.h",
    "crypto/fipsmodule/bn/internal.h",
    "crypto/fipsmodule/bn/rsaz_exp.h",
    "crypto/fipsmodule/cpucap/internal.h",
    "crypto/fipsmodule/cipher/internal.h",
    "crypto/fipsmodule/cpucap/cpu_arm_linux.h",
    "crypto/fipsmodule/delocate.h",
    "crypto/fipsmodule/dh/internal.h",
    "crypto/fipsmodule/digest/internal.h",
    "crypto/fipsmodule/digest/md32_common.h",
    "crypto/fipsmodule/ec/internal.h",
    "crypto/fipsmodule/ec/p256-nistz-table.h",
    "crypto/fipsmodule/ec/p256-nistz.h",
    "crypto/fipsmodule/ec/p256_table.h",
    "crypto/fipsmodule/ec/p384_table.h",
    "crypto/fipsmodule/ec/p521_table.h",
    "crypto/fipsmodule/ecdsa/internal.h",
    "crypto/fipsmodule/evp/internal.h",
    "crypto/fipsmodule/md5/internal.h",
    "crypto/fipsmodule/modes/internal.h",
    "crypto/fipsmodule/rand/fork_detect.h",
    "crypto/fipsmodule/rand/getrandom_fillin.h",
    "crypto/fipsmodule/rand/internal.h",
    "crypto/fipsmodule/rsa/internal.h",
    "crypto/fipsmodule/service_indicator/internal.h",
    "crypto/fipsmodule/sha/internal.h",
    "crypto/dilithium/sig_dilithium.h",
    "crypto/hrss/internal.h",
    "crypto/internal.h",
    "crypto/lhash/internal.h",
    "crypto/obj/obj_dat.h",
    "crypto/pkcs7/internal.h",
    "crypto/pkcs8/internal.h",
    "crypto/poly1305/internal.h",
    "crypto/pool/internal.h",
    "crypto/trust_token/internal.h",
    "crypto/x509/internal.h",
    "crypto/x509v3/ext_dat.h",
    "crypto/x509v3/internal.h",
    "third_party/fiat/curve25519_32.h",
    "third_party/fiat/curve25519_64.h",
    "third_party/fiat/p256_32.h",
    "third_party/fiat/p256_64.h",
    "third_party/s2n-bignum/include/s2n-bignum_aws-lc.h",
    "third_party/s2n-bignum/include/_internal_s2n_bignum.h",
]

_crypto_srcs_generic = [
    "generated-src/err_data.c",
    "crypto/asn1/a_bitstr.c",
    "crypto/asn1/a_bool.c",
    "crypto/asn1/a_d2i_fp.c",
    "crypto/asn1/a_dup.c",
    "crypto/asn1/a_gentm.c",
    "crypto/asn1/a_i2d_fp.c",
    "crypto/asn1/a_int.c",
    "crypto/asn1/a_mbstr.c",
    "crypto/asn1/a_object.c",
    "crypto/asn1/a_octet.c",
    "crypto/asn1/a_strex.c",
    "crypto/asn1/a_strnid.c",
    "crypto/asn1/a_time.c",
    "crypto/asn1/a_type.c",
    "crypto/asn1/a_utctm.c",
    "crypto/asn1/a_utf8.c",
    "crypto/asn1/asn1_lib.c",
    "crypto/asn1/asn1_par.c",
    "crypto/asn1/asn_pack.c",
    "crypto/asn1/f_int.c",
    "crypto/asn1/f_string.c",
    "crypto/asn1/posix_time.c",
    "crypto/asn1/tasn_dec.c",
    "crypto/asn1/tasn_enc.c",
    "crypto/asn1/tasn_fre.c",
    "crypto/asn1/tasn_new.c",
    "crypto/asn1/tasn_typ.c",
    "crypto/asn1/tasn_utl.c",
    "crypto/base64/base64.c",
    "crypto/bio/bio.c",
    "crypto/bio/bio_mem.c",
    "crypto/bio/connect.c",
    "crypto/bio/fd.c",
    "crypto/bio/file.c",
    "crypto/bio/hexdump.c",
    "crypto/bio/pair.c",
    "crypto/bio/printf.c",
    "crypto/bio/socket.c",
    "crypto/bio/socket_helper.c",
    "crypto/blake2/blake2.c",
    "crypto/bn_extra/bn_asn1.c",
    "crypto/bn_extra/convert.c",
    "crypto/buf/buf.c",
    "crypto/bytestring/asn1_compat.c",
    "crypto/bytestring/ber.c",
    "crypto/bytestring/cbb.c",
    "crypto/bytestring/cbs.c",
    "crypto/bytestring/unicode.c",
    "crypto/chacha/chacha.c",
    "crypto/cipher_extra/cipher_extra.c",
    "crypto/cipher_extra/derive_key.c",
    "crypto/cipher_extra/e_aesctrhmac.c",
    "crypto/cipher_extra/e_aesgcmsiv.c",
    "crypto/cipher_extra/e_aes_cbc_hmac_sha1.c",
    "crypto/cipher_extra/e_aes_cbc_hmac_sha256.c",
    "crypto/decrepit/ripemd/ripemd.c",
    "crypto/cipher_extra/e_chacha20poly1305.c",
    "crypto/cipher_extra/e_des.c",
    "crypto/cipher_extra/e_null.c",
    "crypto/cipher_extra/e_rc2.c",
    "crypto/cipher_extra/e_rc4.c",
    "crypto/cipher_extra/e_tls.c",
    "crypto/cipher_extra/tls_cbc.c",
    "crypto/conf/conf.c",
    "crypto/crypto.c",
    "crypto/curve25519/curve25519.c",
    "crypto/curve25519/curve25519_nohw.c",
    "crypto/curve25519/spake25519.c",
    "crypto/des/des.c",
    "crypto/dh_extra/dh_asn1.c",
    "crypto/dh_extra/params.c",
    "crypto/digest_extra/digest_extra.c",
    "crypto/dsa/dsa.c",
    "crypto/dsa/dsa_asn1.c",
    "crypto/ec_extra/ec_asn1.c",
    "crypto/ec_extra/ec_derive.c",
    "crypto/ec_extra/hash_to_curve.c",
    "crypto/ecdh_extra/ecdh_extra.c",
    "crypto/ecdsa_extra/ecdsa_asn1.c",
    "crypto/engine/engine.c",
    "crypto/err/err.c",
    "crypto/evp_extra/evp_asn1.c",
    "crypto/evp_extra/p_dsa_asn1.c",
    "crypto/evp_extra/p_ec_asn1.c",
    "crypto/evp_extra/p_ed25519.c",
    "crypto/evp_extra/p_ed25519_asn1.c",
    "crypto/evp_extra/p_methods.c",
    "crypto/evp_extra/p_kem.c",
    "crypto/evp_extra/p_kem_asn1.c",
    "crypto/evp_extra/p_rsa_asn1.c",
    "crypto/evp_extra/p_x25519.c",
    "crypto/evp_extra/p_x25519_asn1.c",
    "crypto/evp_extra/print.c",
    "crypto/evp_extra/scrypt.c",
    "crypto/evp_extra/sign.c",
    "crypto/ex_data.c",
    "crypto/fipsmodule/bcm.c",
    "crypto/fipsmodule/cpucap/cpucap.c",
    "crypto/fipsmodule/fips_shared_support.c",
    "crypto/hpke/hpke.c",
    "crypto/hrss/hrss.c",
    "crypto/kem/kem.c",
    "crypto/kem/kem_methods.c",
    "crypto/kyber/kem_kyber.c",
    "crypto/rand_extra/pq_custom_randombytes.c",
    "crypto/lhash/lhash.c",
    "crypto/mem.c",
    "crypto/obj/obj.c",
    "crypto/obj/obj_xref.c",
    "crypto/ocsp/ocsp_asn.c",
    "crypto/ocsp/ocsp_client.c",
    "crypto/ocsp/ocsp_extension.c",
    "crypto/ocsp/ocsp_http.c",
    "crypto/ocsp/ocsp_lib.c",
    "crypto/ocsp/ocsp_print.c",
    "crypto/ocsp/ocsp_server.c",
    "crypto/ocsp/ocsp_verify.c",
    "crypto/pem/pem_all.c",
    "crypto/pem/pem_info.c",
    "crypto/pem/pem_lib.c",
    "crypto/pem/pem_oth.c",
    "crypto/pem/pem_pk8.c",
    "crypto/pem/pem_pkey.c",
    "crypto/pem/pem_x509.c",
    "crypto/pem/pem_xaux.c",
    "crypto/pkcs7/pkcs7.c",
    "crypto/pkcs7/pkcs7_x509.c",
    "crypto/pkcs8/p5_pbev2.c",
    "crypto/pkcs8/pkcs8.c",
    "crypto/pkcs8/pkcs8_x509.c",
    "crypto/poly1305/poly1305.c",
    "crypto/poly1305/poly1305_arm.c",
    "crypto/poly1305/poly1305_vec.c",
    "crypto/pool/pool.c",
    "crypto/rand_extra/deterministic.c",
    "crypto/rand_extra/forkunsafe.c",
    "crypto/rand_extra/fuchsia.c",
    "crypto/rand_extra/rand_extra.c",
    "crypto/rand_extra/windows.c",
    "crypto/rc4/rc4.c",
    "crypto/refcount_c11.c",
    "crypto/refcount_lock.c",
    "crypto/rsa_extra/rsa_asn1.c",
    "crypto/rsa_extra/rsa_print.c",
    "crypto/rsa_extra/rsassa_pss_asn1.c",
    "crypto/siphash/siphash.c",
    "crypto/stack/stack.c",
    "crypto/thread.c",
    "crypto/thread_none.c",
    "crypto/thread_pthread.c",
    "crypto/thread_win.c",
    "crypto/trust_token/pmbtoken.c",
    "crypto/trust_token/trust_token.c",
    "crypto/trust_token/voprf.c",
    "crypto/x509/a_digest.c",
    "crypto/x509/a_sign.c",
    "crypto/x509/a_verify.c",
    "crypto/x509/algorithm.c",
    "crypto/x509/asn1_gen.c",
    "crypto/x509/by_dir.c",
    "crypto/x509/by_file.c",
    "crypto/x509/i2d_pr.c",
    "crypto/x509/name_print.c",
    "crypto/x509/policy.c",
    "crypto/x509/rsa_pss.c",
    "crypto/x509/t_crl.c",
    "crypto/x509/t_req.c",
    "crypto/x509/t_x509.c",
    "crypto/x509/t_x509a.c",
    "crypto/x509/x509.c",
    "crypto/x509/x509_att.c",
    "crypto/x509/x509_cmp.c",
    "crypto/x509/x509_d2.c",
    "crypto/x509/x509_def.c",
    "crypto/x509/x509_ext.c",
    "crypto/x509/x509_lu.c",
    "crypto/x509/x509_obj.c",
    "crypto/x509/x509_req.c",
    "crypto/x509/x509_set.c",
    "crypto/x509/x509_trs.c",
    "crypto/x509/x509_txt.c",
    "crypto/x509/x509_v3.c",
    "crypto/x509/x509_vfy.c",
    "crypto/x509/x509_vpm.c",
    "crypto/x509/x509cset.c",
    "crypto/x509/x509name.c",
    "crypto/x509/x509rset.c",
    "crypto/x509/x509spki.c",
    "crypto/x509/x_algor.c",
    "crypto/x509/x_all.c",
    "crypto/x509/x_attrib.c",
    "crypto/x509/x_crl.c",
    "crypto/x509/x_exten.c",
    "crypto/x509/x_info.c",
    "crypto/x509/x_name.c",
    "crypto/x509/x_pkey.c",
    "crypto/x509/x_pubkey.c",
    "crypto/x509/x_req.c",
    "crypto/x509/x_sig.c",
    "crypto/x509/x_spki.c",
    "crypto/x509/x_val.c",
    "crypto/x509/x_x509.c",
    "crypto/x509/x_x509a.c",
    "crypto/x509v3/v3_akey.c",
    "crypto/x509v3/v3_akeya.c",
    "crypto/x509v3/v3_alt.c",
    "crypto/x509v3/v3_bcons.c",
    "crypto/x509v3/v3_bitst.c",
    "crypto/x509v3/v3_conf.c",
    "crypto/x509v3/v3_cpols.c",
    "crypto/x509v3/v3_crld.c",
    "crypto/x509v3/v3_enum.c",
    "crypto/x509v3/v3_extku.c",
    "crypto/x509v3/v3_genn.c",
    "crypto/x509v3/v3_ia5.c",
    "crypto/x509v3/v3_info.c",
    "crypto/x509v3/v3_int.c",
    "crypto/x509v3/v3_lib.c",
    "crypto/x509v3/v3_ncons.c",
    "crypto/x509v3/v3_ocsp.c",
    "crypto/x509v3/v3_pcons.c",
    "crypto/x509v3/v3_pmaps.c",
    "crypto/x509v3/v3_prn.c",
    "crypto/x509v3/v3_purp.c",
    "crypto/x509v3/v3_skey.c",
    "crypto/x509v3/v3_utl.c",
]

tool_srcs = [
    "tool/args.cc",
    "tool/ciphers.cc",
    "tool/client.cc",
    "tool/const.cc",
    "tool/digest.cc",
    "tool/fd.cc",
    "tool/file.cc",
    "tool/generate_ech.cc",
    "tool/generate_ed25519.cc",
    "tool/genrsa.cc",
    "tool/pkcs12.cc",
    "tool/rand.cc",
    "tool/server.cc",
    "tool/sign.cc",
    "tool/speed.cc",
    "tool/tool.cc",
    "tool/transport_common.cc",
]

tool_hdrs = [
    "tool/internal.h",
    "tool/transport_common.h",
]

_crypto_srcs_linux_aarch64 = [
    "generated-src/linux-aarch64/crypto/chacha/chacha-armv8.S",
    "generated-src/linux-aarch64/crypto/cipher_extra/chacha20_poly1305_armv8.S",
    "generated-src/linux-aarch64/crypto/fipsmodule/aesv8-armx.S",
    "generated-src/linux-aarch64/crypto/fipsmodule/aesv8-gcm-armv8.S",
    "generated-src/linux-aarch64/crypto/fipsmodule/aesv8-gcm-armv8-unroll8.S",
    "generated-src/linux-aarch64/crypto/fipsmodule/armv8-mont.S",
    "generated-src/linux-aarch64/crypto/fipsmodule/bn-armv8.S",
    "generated-src/linux-aarch64/crypto/fipsmodule/ghash-neon-armv8.S",
    "generated-src/linux-aarch64/crypto/fipsmodule/ghashv8-armx.S",
    "generated-src/linux-aarch64/crypto/fipsmodule/keccak1600-armv8.S",
    "generated-src/linux-aarch64/crypto/fipsmodule/md5-armv8.S",
    "generated-src/linux-aarch64/crypto/fipsmodule/p256-armv8-asm.S",
    "generated-src/linux-aarch64/crypto/fipsmodule/p256_beeu-armv8-asm.S",
    "generated-src/linux-aarch64/crypto/fipsmodule/sha1-armv8.S",
    "generated-src/linux-aarch64/crypto/fipsmodule/sha256-armv8.S",
    "generated-src/linux-aarch64/crypto/fipsmodule/sha512-armv8.S",
    "generated-src/linux-aarch64/crypto/fipsmodule/vpaes-armv8.S",
]

_crypto_srcs_p384_bignum = select({
    "@platforms//cpu:arm64": [
        "third_party/s2n-bignum/arm/p384/bignum_add_p384.S",
        "third_party/s2n-bignum/arm/p384/bignum_bigendian_6.S",
        "third_party/s2n-bignum/arm/p384/bignum_cmul_p384.S",
        "third_party/s2n-bignum/arm/p384/bignum_deamont_p384.S",
        "third_party/s2n-bignum/arm/p384/bignum_demont_p384.S",
        "third_party/s2n-bignum/arm/p384/bignum_double_p384.S",
        "third_party/s2n-bignum/arm/p384/bignum_half_p384.S",
        "third_party/s2n-bignum/arm/p384/bignum_littleendian_6.S",
        "third_party/s2n-bignum/arm/p384/bignum_mod_n384_6.S",
        "third_party/s2n-bignum/arm/p384/bignum_mod_n384.S",
        "third_party/s2n-bignum/arm/p384/bignum_mod_p384_6.S",
        "third_party/s2n-bignum/arm/p384/bignum_mod_p384.S",
        "third_party/s2n-bignum/arm/p384/bignum_montmul_p384_alt.S",
        "third_party/s2n-bignum/arm/p384/bignum_montmul_p384.S",
        "third_party/s2n-bignum/arm/p384/bignum_montsqr_p384_alt.S",
        "third_party/s2n-bignum/arm/p384/bignum_montsqr_p384.S",
        "third_party/s2n-bignum/arm/p384/bignum_mux_6.S",
        "third_party/s2n-bignum/arm/p384/bignum_neg_p384.S",
        "third_party/s2n-bignum/arm/p384/bignum_nonzero_6.S",
        "third_party/s2n-bignum/arm/p384/bignum_optneg_p384.S",
        "third_party/s2n-bignum/arm/p384/bignum_sub_p384.S",
        "third_party/s2n-bignum/arm/p384/bignum_tomont_p384.S",
        "third_party/s2n-bignum/arm/p384/bignum_triple_p384.S",
        "third_party/s2n-bignum/arm/p384/p384_montjadd.S",
        "third_party/s2n-bignum/arm/p384/p384_montjdouble.S",
        "third_party/s2n-bignum/arm/p384/p384_montjmixadd.S",
    ],
    "@platforms//cpu:x86_64": [
        "third_party/s2n-bignum/x86_att/p384/bignum_add_p384.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_bigendian_6.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_cmul_p384_alt.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_cmul_p384.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_deamont_p384_alt.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_deamont_p384.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_demont_p384_alt.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_demont_p384.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_double_p384.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_half_p384.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_littleendian_6.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_mod_n384_6.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_mod_n384_alt.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_mod_n384.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_mod_p384_6.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_mod_p384_alt.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_mod_p384.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_montmul_p384_alt.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_montmul_p384.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_montsqr_p384_alt.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_montsqr_p384.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_mux_6.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_neg_p384.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_nonzero_6.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_optneg_p384.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_sub_p384.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_tomont_p384_alt.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_tomont_p384.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_triple_p384_alt.S",
        "third_party/s2n-bignum/x86_att/p384/bignum_triple_p384.S",
        "third_party/s2n-bignum/x86_att/p384/p384_montjadd.S",
        "third_party/s2n-bignum/x86_att/p384/p384_montjdouble.S",
        "third_party/s2n-bignum/x86_att/p384/p384_montjmixadd.S",
    ],
})

_crypto_srcs_p521_bignum = select({
    "@platforms//cpu:arm64": [
        "third_party/s2n-bignum/arm/p521/bignum_add_p521.S",
        "third_party/s2n-bignum/arm/p521/bignum_cmul_p521.S",
        "third_party/s2n-bignum/arm/p521/bignum_deamont_p521.S",
        "third_party/s2n-bignum/arm/p521/bignum_demont_p521.S",
        "third_party/s2n-bignum/arm/p521/bignum_double_p521.S",
        "third_party/s2n-bignum/arm/p521/bignum_fromlebytes_p521.S",
        "third_party/s2n-bignum/arm/p521/bignum_half_p521.S",
        "third_party/s2n-bignum/arm/p521/bignum_mod_n521_9.S",
        "third_party/s2n-bignum/arm/p521/bignum_mod_p521_9.S",
        "third_party/s2n-bignum/arm/p521/bignum_montmul_p521_alt.S",
        "third_party/s2n-bignum/arm/p521/bignum_montmul_p521.S",
        "third_party/s2n-bignum/arm/p521/bignum_montsqr_p521_alt.S",
        "third_party/s2n-bignum/arm/p521/bignum_montsqr_p521.S",
        "third_party/s2n-bignum/arm/p521/bignum_mul_p521_alt.S",
        "third_party/s2n-bignum/arm/p521/bignum_mul_p521.S",
        "third_party/s2n-bignum/arm/p521/bignum_neg_p521.S",
        "third_party/s2n-bignum/arm/p521/bignum_optneg_p521.S",
        "third_party/s2n-bignum/arm/p521/bignum_sqr_p521_alt.S",
        "third_party/s2n-bignum/arm/p521/bignum_sqr_p521.S",
        "third_party/s2n-bignum/arm/p521/bignum_sub_p521.S",
        "third_party/s2n-bignum/arm/p521/bignum_tolebytes_p521.S",
        "third_party/s2n-bignum/arm/p521/bignum_tomont_p521.S",
        "third_party/s2n-bignum/arm/p521/bignum_triple_p521.S",
        "third_party/s2n-bignum/arm/p521/p521_jadd.S",
        "third_party/s2n-bignum/arm/p521/p521_jdouble.S",
        "third_party/s2n-bignum/arm/p521/p521_jmixadd.S",
    ],
    "@platforms//cpu:x86_64": [
        "third_party/s2n-bignum/x86_att/p521/bignum_add_p521.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_cmul_p521_alt.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_cmul_p521.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_deamont_p521.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_demont_p521.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_double_p521.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_fromlebytes_p521.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_half_p521.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_mod_n521_9_alt.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_mod_n521_9.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_mod_p521_9.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_montmul_p521_alt.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_montmul_p521.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_montsqr_p521_alt.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_montsqr_p521.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_mul_p521_alt.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_mul_p521.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_neg_p521.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_optneg_p521.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_sqr_p521_alt.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_sqr_p521.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_sub_p521.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_tolebytes_p521.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_tomont_p521.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_triple_p521_alt.S",
        "third_party/s2n-bignum/x86_att/p521/bignum_triple_p521.S",
        "third_party/s2n-bignum/x86_att/p521/p521_jadd.S",
        "third_party/s2n-bignum/x86_att/p521/p521_jdouble.S",
        "third_party/s2n-bignum/x86_att/p521/p521_jmixadd.S",
    ],
})

_crypto_srcs_curve25519_bignum = select({
    "@platforms//cpu:arm64": [
        "third_party/s2n-bignum/arm/curve25519/curve25519_x25519_alt.S",
        "third_party/s2n-bignum/arm/curve25519/curve25519_x25519base_alt.S",
        "third_party/s2n-bignum/arm/curve25519/curve25519_x25519base_byte_alt.S",
        "third_party/s2n-bignum/arm/curve25519/curve25519_x25519base_byte.S",
        "third_party/s2n-bignum/arm/curve25519/curve25519_x25519base.S",
        "third_party/s2n-bignum/arm/curve25519/curve25519_x25519_byte_alt.S",
        "third_party/s2n-bignum/arm/curve25519/curve25519_x25519_byte.S",
        "third_party/s2n-bignum/arm/curve25519/curve25519_x25519.S",
    ],
    "@platforms//cpu:x86_64": [
        "third_party/s2n-bignum/x86_att/curve25519/curve25519_x25519.S",
        "third_party/s2n-bignum/x86_att/curve25519/curve25519_x25519_alt.S",
        "third_party/s2n-bignum/x86_att/curve25519/curve25519_x25519base.S",
        "third_party/s2n-bignum/x86_att/curve25519/curve25519_x25519base_alt.S",
    ],
})

_crypto_srcs_linux_x86_64 = [
    "generated-src/linux-x86_64/crypto/chacha/chacha-x86_64.S",
    "generated-src/linux-x86_64/crypto/cipher_extra/aes128gcmsiv-x86_64.S",
    "generated-src/linux-x86_64/crypto/cipher_extra/aesni-sha1-x86_64.S",
    "generated-src/linux-x86_64/crypto/cipher_extra/aesni-sha256-x86_64.S",
    "generated-src/linux-x86_64/crypto/cipher_extra/chacha20_poly1305_x86_64.S",
    "generated-src/linux-x86_64/crypto/fipsmodule/aesni-gcm-avx512.S",
    "generated-src/linux-x86_64/crypto/fipsmodule/aesni-gcm-x86_64.S",
    "generated-src/linux-x86_64/crypto/fipsmodule/aesni-x86_64.S",
    "generated-src/linux-x86_64/crypto/fipsmodule/aesni-xts-avx512.S",
    "generated-src/linux-x86_64/crypto/fipsmodule/ghash-ssse3-x86_64.S",
    "generated-src/linux-x86_64/crypto/fipsmodule/ghash-x86_64.S",
    "generated-src/linux-x86_64/crypto/fipsmodule/md5-x86_64.S",
    "generated-src/linux-x86_64/crypto/fipsmodule/p256-x86_64-asm.S",
    "generated-src/linux-x86_64/crypto/fipsmodule/p256_beeu-x86_64-asm.S",
    "generated-src/linux-x86_64/crypto/fipsmodule/rdrand-x86_64.S",
    "generated-src/linux-x86_64/crypto/fipsmodule/rsaz-avx2.S",
    "generated-src/linux-x86_64/crypto/fipsmodule/sha1-x86_64.S",
    "generated-src/linux-x86_64/crypto/fipsmodule/sha256-x86_64.S",
    "generated-src/linux-x86_64/crypto/fipsmodule/sha512-x86_64.S",
    "generated-src/linux-x86_64/crypto/fipsmodule/vpaes-x86_64.S",
    "generated-src/linux-x86_64/crypto/fipsmodule/x86_64-mont.S",
    "generated-src/linux-x86_64/crypto/fipsmodule/x86_64-mont5.S",
    "crypto/hrss/asm/poly_rq_mul.S",
]

_crypto_srcs = _crypto_srcs_generic + select({
    "@platforms//cpu:arm64": _crypto_srcs_linux_aarch64,
    "@platforms//cpu:x86_64": _crypto_srcs_linux_x86_64,
})

crypto_srcs = _crypto_srcs + _crypto_srcs_p384_bignum + _crypto_srcs_p521_bignum + _crypto_srcs_curve25519_bignum
