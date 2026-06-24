file(READ ${INPUT_FILE} CONTENT)
file(WRITE ${OUTPUT_FILE}
        "#pragma once\n"
        "inline constexpr char ${VAR_NAME}[] = R\"(\n"
        "${CONTENT}"
        ")\";\n"
)