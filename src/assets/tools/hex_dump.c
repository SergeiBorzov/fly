#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

typedef struct
{
  const char*  inputFilePath;
  char*        outputFilePath;
  char*        arrayName;
  char*        arrayNameUpper;
  unsigned int alignment;
} Input;

static int IsPowerOfTwo(unsigned long x)
{
  return x != 0 && (x & (x - 1)) == 0;
}

static void ToUpper(const char* str, size_t strLen, char* buffer, size_t bufferSize)
{
  size_t i = 0;
  for (i = 0; i < strLen; i++)
  {
    int c     = (int)(str[i]);
    buffer[i] = (char)(toupper(c));
  }
  buffer[i] = '\0';
}

static void ParseCommandLine(int argc, const char* argv[], Input* data)
{
  data->inputFilePath  = NULL;
  data->outputFilePath = NULL;
  data->arrayName       = NULL;
  data->arrayNameUpper = NULL;
  data->alignment        = 0U;

  for (int i = 0; i < argc; i++)
  {
    if (strcmp(argv[i], "-i") == 0)
    {
      if (i + 1 >= argc)
      {
        fprintf(stderr, "No input specified after option\n");
        exit(-1);
      }
      data->inputFilePath = argv[++i];
    }
    else if (strcmp(argv[i], "-o") == 0)
    {
      if (i + 1 >= argc)
      {
        fprintf(stderr, "No output specified after option\n");
        exit(-1);
      }
      const char* outputFilePath = argv[++i];
      size_t      len              = strlen(outputFilePath);
      data->outputFilePath       = (char*)malloc(len + 1);
      strncpy(data->outputFilePath, outputFilePath, len);
    }
    else if (strcmp(argv[i], "-a") == 0)
    {
      if (i + 1 >= argc)
      {
        fprintf(stderr, "No output specified after option\n");
        exit(-1);
      }
      char* end       = NULL;
      data->alignment = strtoul(argv[++i], &end, 10);
      if (errno != 0)
      {
        perror("strtoul");
        exit(-1);
      }
    }
    else if (strcmp(argv[i], "-n") == 0)
    {
      if (i + 1 >= argc)
      {
        fprintf(stderr, "No array name specified after option\n");
        exit(-1);
      }
      const char* arrayName = argv[++i];
      size_t      len        = strlen(arrayName);
      data->arrayName       = (char*)malloc(len + 1);
      strncpy(data->arrayName, arrayName, len);
    }
  }
}

static void DefaultOutputPath(const char* str, size_t strLen, char** buffer)
{
  if (!buffer)
  {
    return;
  }

  size_t nameLen = strLen;
  char*  dot      = strrchr(str, '.');
  if (dot)
  {
    nameLen = (size_t)(dot - str);
  }

  size_t bufferSize = nameLen + 4 + 1;

  *buffer = (char*)malloc(bufferSize);
  strncpy(*buffer, str, nameLen);
  strncat(*buffer, ".inl", bufferSize - nameLen - 1);
}

static void DefaultArrayName(const char* str, size_t strLen, char** buffer)
{
  if (!buffer)
  {
    return;
  }

  size_t nameLen = strLen;
  char*  dot      = strrchr(str, '.');
  if (dot)
  {
    nameLen = (size_t)(dot - str);
  }

  size_t bufferSize = nameLen + 1;

  *buffer = (char*)malloc(bufferSize);
  strncpy(*buffer, str, nameLen);
}

static void ArrayNameUpper(const char* str, size_t strLen, char** buffer)
{
  size_t bufferSize = strLen + 1;
  *buffer            = (char*)malloc(bufferSize);
  ToUpper(str, strLen, *buffer, bufferSize);
}

int main(int argc, char** argv)
{
  Input input;
  ParseCommandLine(argc, (const char**)argv, &input);

  if (!input.inputFilePath)
  {
    fprintf(stderr, "No input specified after option\n");
    return -1;
  }

  if (input.alignment != 0 && !IsPowerOfTwo(input.alignment))
  {
    fprintf(stderr, "Alignment number specified should be power of two!\n");
    return -2;
  }

  if (!input.outputFilePath)
  {
    DefaultOutputPath(input.inputFilePath,
                      strlen(input.inputFilePath),
                      &input.outputFilePath);
  }

  if (!input.arrayName)
  {
    DefaultArrayName(input.outputFilePath, strlen(input.outputFilePath), &input.arrayName);
  }

  ArrayNameUpper(input.arrayName, strlen(input.arrayName), &input.arrayNameUpper);

  FILE* inputStream = fopen(input.inputFilePath, "rb");
  if (!inputStream)
  {
    fprintf(stderr, "Failed to open input file %s\n", input.inputFilePath);
    return -3;
  }

  FILE* outputStream = fopen(input.outputFilePath, "w");
  if (!outputStream)
  {
    fprintf(stderr, "Failed to open output file %s\n", input.outputFilePath);
    return -3;
  }

  const unsigned long maxBytesPerRow = 16;
  size_t              totalByteSize   = 0;

  if (input.alignment == 0)
  {
    fprintf(outputStream, "const unsigned char %s[] = {", input.arrayName);
  }
  else
  {
    fprintf(outputStream,
            "alignas(%u) const unsigned char %s[] = {",
            input.alignment,
            input.arrayName);
  }

  int next_c = fgetc(inputStream);
  while (next_c != EOF)
  {
    int curr_c = next_c;
    next_c     = fgetc(inputStream);

    unsigned char byte = (unsigned char)curr_c;

    if (totalByteSize % maxBytesPerRow == 0)
    {
      fprintf(outputStream, "\n    ");
    }

    fprintf(outputStream, "0x%02x", byte);

    if (next_c != EOF)
    {
      fprintf(outputStream, ", ");
    }

    ++totalByteSize;
  }
  if (totalByteSize % maxBytesPerRow == 0)
  {
    fprintf(outputStream, "\n");
  }
  fprintf(outputStream, "};\n");
  fprintf(outputStream, "#define %s_SIZE (%zuULL)\n", input.arrayNameUpper, totalByteSize);
  fprintf(outputStream, "\n");

  free(input.arrayNameUpper);
  free(input.arrayName);
  free(input.outputFilePath);

  fclose(outputStream);
  fclose(inputStream);

  return 0;
}
