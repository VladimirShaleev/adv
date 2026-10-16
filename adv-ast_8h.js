var adv_ast_8h =
[
    [ "adv_node_t", "group__adv__types.html#ga2c47cf9588de4c8f068ce1be8ac45049", null ],
    [ "adv_node_type_t", "group__adv__enums.html#gaed1bf1d656039f0879ab36ba78d7c42f", [
      [ "ADV_NODE_UNKNOWN", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42faa1b0d749c1aee855992da6a360f93701", null ],
      [ "ADV_NODE_PROGRAM", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fa4f55559c272a549c90b3b16f40d7c98d", null ],
      [ "ADV_NODE_BLOCK", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fa2c721636979babb5770ce8db95df8b19", null ],
      [ "ADV_NODE_VAR_DECL", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fad9c8d19227d25eec7edb13f6db145a48", null ],
      [ "ADV_NODE_NATIVE_DECL", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fa968c2c2bc7e0576f56f52b74d0334118", null ],
      [ "ADV_NODE_FUNCTION_DECL", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fa88d8259690ac271d5cc5cf9391f775f5", null ],
      [ "ADV_NODE_PARAM_DECL", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fab2b554ba6b47b67e38c50ad96741babc", null ],
      [ "ADV_NODE_IMPORT_STMT", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fa9547524136cce5e633009c2331bb830d", null ],
      [ "ADV_NODE_IF_STMT", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fa378a9f77c931ae1fa4ca5f0f9d2478bc", null ],
      [ "ADV_NODE_WHILE_STMT", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fa37630898f2c6808a857d07e65a4e6db2", null ],
      [ "ADV_NODE_RETURN_STMT", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42faac112c79c71334334560846c2832a40a", null ],
      [ "ADV_NODE_YIELD_STMT", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fa5f60cc4fadaf9634064b95a752cc6823", null ],
      [ "ADV_NODE_WAIT_STMT", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fa5228c27ca79668efe0ef80a203db9a00", null ],
      [ "ADV_NODE_EXPR_STMT", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fa0e5082403ddceff2eecef3071aad43e1", null ],
      [ "ADV_NODE_IMPLICIT_CAST_EXPR", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fa9542f807011fef4286df39be9296ed98", null ],
      [ "ADV_NODE_EXPLICIT_CAST_EXPR", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fa37312242f54dbdff931212e7252b9a32", null ],
      [ "ADV_NODE_IDENTIFIER_EXPR", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fa2088623d271642bba73535224b837e11", null ],
      [ "ADV_NODE_ASSIGN_EXPR", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fad3cde02a6de3d7a3de40f191cc5e35e5", null ],
      [ "ADV_NODE_BINARY_EXPR", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42faae27fb1af880236626aaaa37fed0fe9e", null ],
      [ "ADV_NODE_UNARY_EXPR", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42faf317fef092091f7c9e8657f96137761e", null ],
      [ "ADV_NODE_CALL_EXPR", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fa9ca7bad26f025879137d0fc4e03226ab", null ],
      [ "ADV_NODE_MEMBER_ACCESS", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42faa0633914c6f66b5a341d6e79ed2ea13a", null ],
      [ "ADV_NODE_BOOL_LITERAL", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fa1b1b21ffe1ef760b8350fb304e6adb86", null ],
      [ "ADV_NODE_INT_LITERAL", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42faa2500cf24f4a0503aa822b8ab5e76fe8", null ],
      [ "ADV_NODE_FLOAT_LITERAL", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fa9d4f4ca5341b203de4b5652222ccc309", null ],
      [ "ADV_NODE_STRING_LITERAL", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fa5962ca044cb1a84aea4078f2e0fdf7c3", null ],
      [ "ADV_NODE_COUNT_ENUMS", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42fa57cb863beb03f9e117d2e93302fa0111", null ],
      [ "ADV_NODE_MAX_ENUM", "group__adv__enums.html#ggaed1bf1d656039f0879ab36ba78d7c42facecedd92d58839d64354f30c5d174ed3", null ]
    ] ]
];