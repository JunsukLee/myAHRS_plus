/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2014 ARM Limited
 *     ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "Text.h"
#include "Shader.h"
#include "Platform.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace MaliSDK
{
    const char Text::textureFilename[]        = "font.raw";
    const char Text::vertexShaderFilename[]   = "font.vert";
    const char Text::fragmentShaderFilename[] = "font.frag";

    const float Text::scale = 1.0f;

    const int Text::textureCharacterWidth = 8;
    const int Text::textureCharacterHeight = 16;

    /* Please see header for specification. */
    void loadData(const char* filename, unsigned char** textureData)
    {
        LOGI("Texture loadData started for %s...\n", filename);

        FILE* file = fopen(filename, "rb");

        if (file == NULL)
        {
            LOGE("Failed to open '%s'\n", filename);
            exit(EXIT_FAILURE);
        }

        fseek(file, 0, SEEK_END);

        unsigned int   length = ftell(file);
        unsigned char* loadedTexture = NULL;

        MALLOC_CHECK(unsigned char*, loadedTexture, length);

        fseek(file, 0, SEEK_SET);

        size_t read = fread(loadedTexture, sizeof(unsigned char), length, file);

        if (read != length)
        {
            LOGE("Failed to read in '%s'\n", filename);
            exit(EXIT_FAILURE);
        }

        fclose(file);

        *textureData = loadedTexture;
    }

    /* Please see header for specification. */
    GLint get_and_check_attrib_location(GLuint program, const GLchar* attrib_name)
    {
        GLint attrib_location = GL_CHECK(glGetAttribLocation(program, attrib_name));

        if (attrib_location == -1)
        {
            LOGE("Cannot retrieve location of %s attribute.\n", attrib_name);
            exit(EXIT_FAILURE);
        }

        return attrib_location;
    }

    /* Please see header for specification. */
    GLint get_and_check_uniform_location(GLuint program, const GLchar* uniform_name)
    {
        GLint uniform_location = GL_CHECK(glGetUniformLocation(program, uniform_name));

        if (uniform_location == -1)
        {
            LOGE("Cannot retrieve location of %s uniform.\n", uniform_name);
            exit(EXIT_FAILURE);
        }

        return uniform_location;
    }

    /* Please see header for specification. */
    Text::Text(const char* resourceDirectory, int windowWidth, int windowHeight)
    {
        vertexShaderID     = 0;
        fragmentShaderID   = 0;
        programID          = 0;
        numberOfCharacters = 0;

        textVertex             = NULL;
        textTextureCoordinates = NULL;
        color                  = NULL;
        textIndex              = NULL;

        /* Create an orthographic projection. */
        projectionMatrix = Matrix::matrixOrthographic(0, (float)windowWidth, 0, (float)windowHeight, 0, 1);

        /* Shaders. */
        const int vertexShaderFilenameLength = strlen(resourceDirectory) + strlen(vertexShaderFilename) + 1;
        char*     vertexShader               = NULL;

        MALLOC_CHECK(char*, vertexShader, vertexShaderFilenameLength);
        strcpy(vertexShader, resourceDirectory);
        strcat(vertexShader, vertexShaderFilename);

        Shader::processShader(&vertexShaderID, vertexShader, GL_VERTEX_SHADER);

        FREE_CHECK(vertexShader);

        const int fragmentShaderFilenameLength = strlen(resourceDirectory) + strlen(fragmentShaderFilename) + 1;
        char*     fragmentShader               = NULL;

        MALLOC_CHECK(char*, fragmentShader, fragmentShaderFilenameLength);
        strcpy(fragmentShader, resourceDirectory);
        strcat(fragmentShader, fragmentShaderFilename);

        Shader::processShader(&fragmentShaderID, fragmentShader, GL_FRAGMENT_SHADER);

        FREE_CHECK(fragmentShader);

        /* Set up shaders. */
        programID = GL_CHECK(glCreateProgram());

        GL_CHECK(glAttachShader(programID, vertexShaderID));
        GL_CHECK(glAttachShader(programID, fragmentShaderID));
        GL_CHECK(glLinkProgram(programID));
        GL_CHECK(glUseProgram(programID));

        /* Vertex positions. */
        m_iLocPosition = get_and_check_attrib_location(programID, "a_v4Position");

        /* Text colors. */
        m_iLocTextColor = get_and_check_attrib_location(programID, "a_v4FontColor");

        /* TexCoords. */
        m_iLocTexCoord = get_and_check_attrib_location(programID, "a_v2TexCoord");

        /* Projection matrix. */
        m_iLocProjection = get_and_check_uniform_location(programID, "u_m4Projection");

        GL_CHECK(glUniformMatrix4fv(m_iLocProjection, 1, GL_FALSE, projectionMatrix.getAsArray()));

        /* Set the sampler to point at the 0th texture unit. */
        m_iLocTexture = get_and_check_uniform_location(programID, "u_s2dTexture");

        GL_CHECK(glUniform1i(m_iLocTexture, 0));

        /* Load texture. */
        GL_CHECK(glActiveTexture(GL_TEXTURE0));
        GL_CHECK(glGenTextures(1, &textureID));
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureID));

        /* Set filtering. */
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

        const int textureLength = strlen(resourceDirectory) + strlen(textureFilename) + 1;
        char*     texture       = NULL;

        MALLOC_CHECK(char*, texture, textureLength);
        strcpy(texture, resourceDirectory);
        strcat(texture, textureFilename);

        unsigned char* textureData = NULL;

        loadData(texture, &textureData);

        FREE_CHECK(texture);

        GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 48, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData));

        FREE_CHECK(textureData);
    }

    /* Please see header for specification. */
    void Text::clear(void)
    {
        numberOfCharacters = 0;

        FREE_CHECK(textVertex);
        FREE_CHECK(textTextureCoordinates);
        FREE_CHECK(color);
        FREE_CHECK(textIndex);
    }

    /* Please see header for specification. */
    void Text::addString(int xPosition, int yPosition, const char* string, int red, int green, int blue, int alpha)
    {
        int length = (int)strlen(string);
        int iTexCoordPos = 4 * 2 * numberOfCharacters;
        int iVertexPos = 4 * 3 * numberOfCharacters;
        int iColorPos = 4 * 4 * numberOfCharacters;
        int iIndex = 0;
        int iIndexPos = 0;

        numberOfCharacters += length;

        /* Realloc memory. */
        REALLOC_CHECK(float*,   textVertex,             numberOfCharacters * 4 * 3 * sizeof(float));
        REALLOC_CHECK(float*,   textTextureCoordinates, numberOfCharacters * 4 * 2 * sizeof(float));
        REALLOC_CHECK(float*,   color,                  numberOfCharacters * 4 * 4 * sizeof(float));
        REALLOC_CHECK(GLshort*, textIndex,              (numberOfCharacters * 6 - 2) * sizeof(GLshort));

        /* Re-init entire index array. */
        textIndex[iIndex++] = 0;
        textIndex[iIndex++] = 1;
        textIndex[iIndex++] = 2;
        textIndex[iIndex++] = 3;

        iIndexPos = 4;
        for (int cIndex = 1; cIndex < numberOfCharacters; cIndex ++)
        {
            textIndex[iIndexPos++] = iIndex - 1;
            textIndex[iIndexPos++] = iIndex;
            textIndex[iIndexPos++] = iIndex++;
            textIndex[iIndexPos++] = iIndex++;
            textIndex[iIndexPos++] = iIndex++;
            textIndex[iIndexPos++] = iIndex++;
        }

        for (int iChar = 0; iChar < (signed int)strlen(string); iChar ++)
        {
            char cChar = string[iChar];
            int iCharX = 0;
            int iCharY = 0;
            Vec2 sBottom_left;
            Vec2 sBottom_right;
            Vec2 sTop_left;
            Vec2 sTop_right;

            /* Calculate tex coord for char here. */
            cChar -= 32;
            iCharX = cChar % 32;
            iCharY = cChar / 32;
            iCharX *= textureCharacterWidth;
            iCharY *= textureCharacterHeight;
            sBottom_left.x = iCharX;
            sBottom_left.y = iCharY;
            sBottom_right.x = iCharX + textureCharacterWidth;
            sBottom_right.y = iCharY;
            sTop_left.x = iCharX;
            sTop_left.y = iCharY + textureCharacterHeight;
            sTop_right.x = iCharX + textureCharacterWidth;
            sTop_right.y = iCharY + textureCharacterHeight;

            /* Add vertex position data here. */
            textVertex[iVertexPos++] = xPosition + iChar * textureCharacterWidth * scale;
            textVertex[iVertexPos++] = (float)yPosition;
            textVertex[iVertexPos++] = 0;

            textVertex[iVertexPos++] = xPosition + (iChar + 1) * textureCharacterWidth * scale;
            textVertex[iVertexPos++] = (float)yPosition;
            textVertex[iVertexPos++] = 0;

            textVertex[iVertexPos++] = xPosition + iChar * textureCharacterWidth * scale;
            textVertex[iVertexPos++] = yPosition + textureCharacterHeight * scale;
            textVertex[iVertexPos++] = 0;

            textVertex[iVertexPos++] = xPosition + (iChar + 1) * textureCharacterWidth * scale;
            textVertex[iVertexPos++] = yPosition + textureCharacterHeight * scale;
            textVertex[iVertexPos++] = 0;

            /* Texture coords here. Because textures are read in upside down, flip Y coords here. */
            textTextureCoordinates[iTexCoordPos++] = sBottom_left.x / 256.0f;
            textTextureCoordinates[iTexCoordPos++] = sTop_left.y / 48.0f;

            textTextureCoordinates[iTexCoordPos++] = sBottom_right.x / 256.0f;
            textTextureCoordinates[iTexCoordPos++] = sTop_right.y / 48.0f;

            textTextureCoordinates[iTexCoordPos++] = sTop_left.x / 256.0f;
            textTextureCoordinates[iTexCoordPos++] = sBottom_left.y / 48.0f;

            textTextureCoordinates[iTexCoordPos++] = sTop_right.x / 256.0f;
            textTextureCoordinates[iTexCoordPos++] = sBottom_right.y / 48.0f;

            /* Color data. */
            color[iColorPos++] = red / 255.0f;
            color[iColorPos++] = green / 255.0f;
            color[iColorPos++] = blue / 255.0f;
            color[iColorPos++] = alpha / 255.0f;

            /* Copy to the other 3 vertices. */
            memcpy(&color[iColorPos],     &color[iColorPos - 4], 4 * sizeof(float));
            memcpy(&color[iColorPos + 4], &color[iColorPos],     4 * sizeof(float));
            memcpy(&color[iColorPos + 8], &color[iColorPos + 4], 4 * sizeof(float));
            iColorPos += 3 * 4;
        }
    }

    /* Please see header for specification. */
    void Text::draw(void)
    {
        /* Push currently bound vertex array object. */
        GLint vertexArray = 0;

        GL_CHECK(glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray));
        GL_CHECK(glBindVertexArray(0));
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));

        /* Push currently used program object. */
        GLint currentProgram = 0;

        GL_CHECK(glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram));

        GL_CHECK(glUseProgram(programID));

        if (m_iLocPosition == -1 || m_iLocTextColor == -1 || m_iLocTexCoord == -1 || m_iLocProjection == -1)
        {
            LOGI("At least one of the attributes and/or uniforms is missing. Have you invoked Text(const char*, int, int) constructor?");
            exit(EXIT_FAILURE);
        }

        if (numberOfCharacters == 0)
        {
            return;
        }

        GL_CHECK(glEnableVertexAttribArray(m_iLocPosition));
        GL_CHECK(glEnableVertexAttribArray(m_iLocTextColor));
        GL_CHECK(glEnableVertexAttribArray(m_iLocTexCoord));

        GL_CHECK(glVertexAttribPointer(m_iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, textVertex));
        GL_CHECK(glVertexAttribPointer(m_iLocTextColor, 4, GL_FLOAT, GL_FALSE, 0, color));
        GL_CHECK(glVertexAttribPointer(m_iLocTexCoord, 2, GL_FLOAT, GL_FALSE, 0, textTextureCoordinates));
        GL_CHECK(glUniformMatrix4fv(m_iLocProjection, 1, GL_FALSE, projectionMatrix.getAsArray()));

        GL_CHECK(glActiveTexture(GL_TEXTURE0));
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureID));

        GL_CHECK(glDrawElements(GL_TRIANGLE_STRIP, numberOfCharacters * 6 - 2, GL_UNSIGNED_SHORT, textIndex));

        GL_CHECK(glDisableVertexAttribArray(m_iLocTextColor));
        GL_CHECK(glDisableVertexAttribArray(m_iLocTexCoord));
        GL_CHECK(glDisableVertexAttribArray(m_iLocPosition));

        /* Pop previously used program object. */
        GL_CHECK(glUseProgram(currentProgram));

        /* Pop previously bound vertex array object. */
        GL_CHECK(glBindVertexArray(vertexArray));
    }

    /* Please see header for specification. */
    Text::Text() :
        m_iLocPosition(-1),
        m_iLocProjection(-1),
        m_iLocTextColor(-1),
        m_iLocTexCoord(-1),
        m_iLocTexture(-1),
        vertexShaderID(0),
        fragmentShaderID(0),
        programID(0),
        textureID(0)
    {
        clear();
    }

    /* Please see header for specification. */
    Text::~Text(void)
    {
        clear();

        GL_CHECK(glDeleteTextures(1, &textureID));
    }
}