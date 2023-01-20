#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <sstream>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <string>
#include <vector>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

// We use the stb image libs for reading, resizing, and writing images for packing.
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#define STB_IMAGE_RESIZE_STATIC
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_STATIC

#include <stb_image_resize.h>
#include <stb_image_write.h>
#include <stb_image.h>

#define MAKEICON_VERSION_MAJOR 1
#define MAKEICON_VERSION_MINOR 3

#define ERROR(...)                       \
do                                       \
{                                        \
fprintf(stderr, "[makeicon] error: ");   \
fprintf(stderr, __VA_ARGS__);            \
fprintf(stderr, "\n");                   \
abort();                                 \
}                                        \
while (0)
#define WARNING(...)                     \
do                                       \
{                                        \
fprintf(stderr, "[makeicon] warning: "); \
fprintf(stderr, __VA_ARGS__);            \
fprintf(stderr, "\n");                   \
}                                        \
while(0)

#define CAST(t,x) ((t)(x))

typedef  uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint32_t u64;
typedef   int8_t  s8;
typedef  int16_t s16;
typedef  int32_t s32;
typedef  int64_t s64;
typedef    float f32;
typedef   double f64;

typedef s32 Platform;
enum Platform_
{
    Platform_Win32,
    Platform_OSX,
    Platform_iOS,
    Platform_Android,
    Platform_COUNT
};

static constexpr const char* PLATFORM_NAMES[Platform_COUNT] = { "win32", "osx", "ios", "android" };

static constexpr const char* MAKEICON_HELP_MESSAGE =
"makeicon [-help] [-version] [-resize] [-platform:name] -sizes:x,y,z... -input:x,y,z... output\n"
"\n"
"    -sizes:...   [Required]  Comma-separated list of icon size(s) to be included in the generated output icon or a .json file to read sizes from on mac.\n"
"    -input:...   [Required]  Comma-separated input image(s) and/or directories and/or .txt files containing file names to be used to generate the icon sizes.\n"
"    -resize      [Optional]  Whether to allow resizing input images to match the requested output sizes, defaults to false.\n"
"    -radius      [Optional]  Round the edges of the icon image by percentage of size, defaults to 0\n"
"    -padding     [Optional]  Adds alpha padding around icon by percentage of size, defaults to 0\n"
"    -platform    [Optional]  Platform to generate icons for. Options are win32, osx, ios, android. Defaults to win32.\n"
"    -version     [Optional]  Prints out the current version number of the makeicon binary and exits.\n"
"    -help        [Optional]  Prints out this help/usage message for the program and exits.\n"
"     output      [Required]  The name of the icon that will be generated by the program.\n";

struct Argument
{
    std::string              name;
    std::vector<std::string> params; // Optional!
};

struct Options
{
    Platform                 platform = Platform_Win32;
    bool                     resize   = false;
    std::vector<s32>         sizes;
    std::vector<std::string> input;
    std::string              contents;
    std::string              output;
    f32                      padding = 0.0f;
    f32                      radius = 0.0f;
};

struct Image
{
    s32 width  = 0;
    s32 height = 0;
    s32 bpp    = 0; // bytes per pixel
    u8* data   = NULL;

    inline bool operator<(const Image& rhs) const
    {
        return ((width*height) < (rhs.width*rhs.height));
    }
};

static void free_image(Image& image)
{
    free(image.data);
    image.data = NULL;
}

struct PngImage
{
    explicit PngImage(const Image& image)
    {
        int mem_size = 0;
        int stride_in_bytes = image.width * 4;
        width = image.width;
        height = image.height;
        data = stbi_write_png_to_mem(image.data, stride_in_bytes, width, height, 4, &mem_size);
        data_size = mem_size;
    }

    s32     width       = 0;
    s32     height      = 0;
    size_t  data_size   = 0;
    u8*     data        = NULL;
};

static void free_png_image(PngImage& png_image)
{
    stbi_image_free(png_image.data);
    png_image.data = NULL;
    png_image.data_size = 0;
}

// returns true on success, false on failure. The resized image is written to `output`.
static bool resize_image(const Image& image, s32 output_width, s32 output_height, Image& output)
{
    output.bpp = image.bpp;
    output.width = output_width;
    output.height = output_height;
    output.data = CAST(u8*, malloc(output_width * output_height * image.bpp));
    if (!output.data)
    {
        return false;
    }
    else
    {
        stbir_resize_uint8_srgb(image.data, image.width, image.height, image.width * image.bpp,
            output.data, output_width, output_height, output_width * image.bpp, image.bpp, 3, 0);
    }
    return true;
}

static void add_border(Image& image, f32 padding)
{
    padding = padding > 0.5f ? 0.5f: padding;
    u32 inner_size_width = (1.0f - 2.0f * padding) * image.width;
    u32 inner_size_height = (1.0f - 2.0f * padding) * image.height;

    u8* padded_data = CAST(u8*, malloc(inner_size_width * inner_size_height * image.bpp));
    stbir_resize_uint8_srgb(image.data, image.width, image.height, image.width * image.bpp,
                            padded_data, inner_size_width, inner_size_height, inner_size_width * image.bpp, image.bpp, 3, 0);

    u32 padded_width = padding * image.width;
    u32 padded_height = padding * image.height;

    u32 output_stride = image.width * image.bpp;
    u32 src_stride = inner_size_width * image.bpp;

    u8* output_dst = image.data + padded_width * image.bpp + output_stride * padded_height;
    u8* src = padded_data;
    memset(image.data, 0, image.width * image.height * image.bpp);
    for (u32 y = 0; y < inner_size_height; ++y)
    {
        memcpy(output_dst, src, inner_size_width * image.bpp);
        output_dst += output_stride;
        src += src_stride;
    }
    free(padded_data);
}

static void apply_radius(u8* data, s32 width, s32 bpp, s32 cx, s32 cy,
                        s32 startX, s32 endX, s32 startY, s32 endY, 
                        s32 radius_squared)
{
    for (u32 y = startY; y < endY; ++y)
    {
        for (u32 x = startX; x < endX; ++x)
        {
            u32 d = (cy - y) * (cy - y) + (cx - x) * (cx - x);

            if (d > radius_squared)
            {
                u32 pos = (y * width + x) * bpp;
                memset(data + pos, 0, bpp);
            }
        }
    }
}

static void add_corner_radius(Image& image, f32 radius)
{
    radius = radius > 0.5f ? 0.5f: radius;

    s32 radius_squared = image.width * radius;
    radius_squared *= radius_squared;               
                                                    
    s32 left = image.width * radius;
    s32 right = image.width - left;
    
    s32 top = image.height * radius;
    s32 bottom = image.height - top;

    apply_radius(image.data, image.width, image.bpp, left, top, 0, left, 0, top, radius_squared); // top left
    apply_radius(image.data, image.width, image.bpp, left, bottom, 0, left, bottom, image.height, radius_squared); // bottom left
    apply_radius(image.data, image.width, image.bpp, right, top, right, image.width, 0, top, radius_squared); // top right
    apply_radius(image.data, image.width, image.bpp, right, bottom, right, image.width, bottom, image.height, radius_squared); // bottom right
}

static void modify_image(Image& image, const Options& options)
{
    if (options.radius > 0.0f)
    {
        add_corner_radius(image, options.radius);
    }

    if (options.padding > 0.0f)
    {
        add_border(image, options.padding);
    }
}

static bool save_image(const Image& image, std::string file_name, s32 resize_width = 0, s32 resize_height = 0)
{
    s32 output_width = (resize_width > 0) ? resize_width : image.width;
    s32 output_height = (resize_height > 0) ? resize_height : image.height;
    Image resized_image;

    // If the resize options were specified then resize first, this also means we need to free output_data after as we allocate new memory.
    bool did_resize_image = false;
    if(output_width != image.width || output_height != image.height)
    {
        if (!resize_image(image, output_width, output_height, resized_image))
        {
            return false;
        }
        did_resize_image = true;
    }

    stbi_write_png(file_name.c_str(), output_width, output_height, image.bpp, did_resize_image ? resized_image.data : image.data, output_width*image.bpp);

    if(did_resize_image)
    {
        free_image(resized_image);
    }

    return true;
}

static void resize_and_save_image(const std::string& filename, const std::vector<Image>& input_images, s32 size, bool resize)
{
    // Search for matching image input size to save out as PNG.
    bool match_found = false;
    for(auto& image: input_images)
    {
        if(image.width == size && image.height == size)
        {
            match_found = true;
            save_image(image, filename);
            break;
        }
    }
    if(!match_found)
    {
        if(!resize)
        {
            // If no match was found and resize wasn't specified then we fail.
            ERROR("Size %d was requested but no input image of this size was provided! Potentially specify -resize to allow for reszing to this size.", size);
        }
        else
        {
            // If no match was found and resize was specified then we resize for this icon size (use the largest image).
            save_image(input_images.at(input_images.size()-1), filename, size,size);
        }
    }
}

static std::vector<u8> read_entire_binary_file(const std::string& file_name)
{
    std::ifstream file(file_name, std::ios::binary);
    std::vector<u8> content;
    content.resize(std::filesystem::file_size(file_name));
    file.read(CAST(char*, &content[0]), content.size()*sizeof(u8));
    return content;
}

static void tokenize_string(const std::string& str, const char* delims, std::vector<std::string>& tokens)
{
    size_t prev = 0;
    size_t pos;

    while((pos = str.find_first_of(delims, prev)) != std::string::npos)
    {
        if(pos > prev) tokens.push_back(str.substr(prev, pos-prev));
        prev = pos+1;
    }
    if(prev < str.length())
    {
        tokens.push_back(str.substr(prev, std::string::npos));
    }
}

static void print_version_message()
{
    fprintf(stdout, "makeicon v%d.%d\n", MAKEICON_VERSION_MAJOR, MAKEICON_VERSION_MINOR);
}

static void print_help_message()
{
    fprintf(stdout, "%s\n", MAKEICON_HELP_MESSAGE);
}

static Argument format_argument(std::string arg_str)
{
    // @Improve: Handle ill-formed argument error cases!

    // Remove the '-' character from the argument.
    arg_str.erase(0,1);
    // Split the argument into its name and (optional) parameters.
    std::vector<std::string> tokens;
    tokenize_string(arg_str, ":", tokens);
    // Store the formatted argument information.
    Argument arg;
    if(!tokens.empty())
    {
        arg.name = tokens[0];
        if(tokens.size() > 1) // We have parameters!
        {
            tokenize_string(tokens[1], ",", arg.params);
        }
    }
    return arg;
}

static s32 make_icon_win32(const Options& options, const std::vector<Image>& input_images);
static s32 make_icon_android(const Options& options, const std::vector<Image>& input_images);
static s32 make_icon_apple(const Options& options, const std::vector<Image>&input_images);

static s32 make_icon(const Options& options)
{
    std::vector<Image> input_images;

    // Load all of the input images into memory.
    for(auto& file_name: options.input)
    {
        Image image;
        image.data = stbi_load(file_name.c_str(), &image.width,&image.height,&image.bpp,4); // We force to 4-channel RGBA.
        image.bpp = 4;
        if(!image.data)
        {
            ERROR("Failed to load input image: %s", file_name.c_str());
        }
        else
        {
            // We warn about non-square images as they will be stretched to a square aspect.
            if(image.width != image.height)
            {
                WARNING("Image file '%s' is not square and will be stretched! Consider changing its size.", file_name.c_str());
            }
            // We warn if two images are passed in with the same size.
            for(auto& input: input_images)
            {
                if((input.width == image.width) && (input.height == image.height))
                {
                    WARNING("Two provided image files have the same siize of %dx%d! It is ambiguous which one will be used.", image.width,image.height);
                    break;
                }
            }

            input_images.push_back(image);
        }
    }

    for (auto& img : input_images)
    {
        modify_image(img, options);
    }

    s32 result = EXIT_FAILURE;

    // Run the icon generation code for the desired platform.
    switch(options.platform)
    {
        case Platform_Win32:
        {
            result = make_icon_win32(options, input_images);
        } break;
        case Platform_OSX:
        case Platform_iOS:
        {
            result = make_icon_apple(options, input_images);
        } break;
        case Platform_Android:
        {
            result = make_icon_android(options, input_images);
        } break;
        default:
        {
            WARNING("Unknown platform ID used for making icon: %d", options.platform);
        } break;
    }

    // Free all of the loaded to avoid memory leaking.
    for(auto& image: input_images)
    {
        free_image(image);
    }

    return result;
}

int main(int argc, char** argv)
{
    Options options;

    // test command line: -input:./icon.png -sizes:256,128,64,32 -resize ./icon.ico

    // Parse command line arguments given to the program, if there are not
    // any arguments provided then it makes sense to print the help message.
    // This parsing process populates the program options structure with
    // information needed in order to perform the desired program task.
    if(argc <= 1)
    {
        print_help_message();
        return EXIT_SUCCESS;
    }
    else
    {
        for(s32 i=1; i<argc; ++i)
        {
            // Handle options.
            if(argv[i][0] == '-')
            {
                Argument arg = format_argument(argv[i]);
                if(arg.name == "resize")
                {
                    options.resize = true;
                }
                else if(arg.name == "sizes")
                {
                    for(auto& param: arg.params)
                    {
                        // allows sizes to be specified through a json file (used for apple icon generation)
                        if(param.find(".json") != -1) options.contents = param;
                        else options.sizes.push_back(std::stoi(param));
                    }
                    if(options.sizes.empty() && options.contents.empty())
                    {
                        ERROR("No sizes provided with -sizes argument!");
                    }
                }
                else if(arg.name == "input")
                {
                    for(auto& param: arg.params)
                    {
                        if(std::filesystem::is_directory(param))
                        {
                            for(auto& p: std::filesystem::directory_iterator(param))
                            {
                                if(std::filesystem::is_regular_file(p))
                                {
                                    options.input.push_back(p.path().string());
                                }
                            }
                        }
                        else
                        {
                            if(std::filesystem::path(param).extension() == ".txt")
                            {
                               // If it's a text file we read each line and add those as file names for input.
                                std::ifstream file(param);
                                if(!file.is_open())
                                {
                                    ERROR("Failed to read .txt file passed in as input: %s", param.c_str());
                                }
                                else
                                {
                                    std::string line;
                                    while(getline(file, line))
                                    {
                                        if(std::filesystem::is_regular_file(line))
                                        {
                                            options.input.push_back(line);
                                        }
                                    }
                                }
                            }
                            else
                            {
                                options.input.push_back(param);
                            }
                        }
                    }
                    if(options.input.empty())
                    {
                        ERROR("No input provided with -input argument!");
                    }
                }
                else if(arg.name == "platform")
                {
                    std::string platform = arg.params[0];
                    for(Platform i=0; i<Platform_COUNT; ++i)
                    {
                        if(platform == PLATFORM_NAMES[i])
                        {
                            options.platform = i;
                            break;
                        }
                    }
                }
                else if (arg.name == "padding")
                {
                    for (auto& param : arg.params)
                    {
                        options.padding = std::stof(param);
                    }
                }
                else if (arg.name == "radius")
                {
                    for (auto& param : arg.params)
                    {
                        options.radius = std::stof(param);
                    }
                }
                else if(arg.name == "version")
                {
                    print_version_message();
                    return EXIT_SUCCESS;
                }
                else if(arg.name == "help")
                {
                    print_help_message();
                    return EXIT_SUCCESS;
                }
                else
                {
                    ERROR("Unknown argument: %s", arg.name.c_str());
                }
            }
            else // Handle output.
            {
                // If there are still arguments/options after the final output name parameter then we consider
                // the input ill-formed and we inform the user of how to format the arguments to the program.
                if(i < (argc-1))
                {
                    ERROR("Extra arguments after final '%s' parameter!", argv[i]);
                }
                else
                {
                    options.output = argv[i];
                }
            }
        }
    }

    // Ensure the options structure is populated with valid data. If there are no inputs, sizes, or ouput we cannot run!
    if(options.sizes.empty() && options.contents.empty())
        ERROR("No icon sizes provided! Specify sizes using: -sizes:x,y,z,w...");
    if(options.input.empty())
        ERROR("No input images provided! Specify input using: -input:x,y,z,w...");
    if(options.output.empty())
        ERROR("No output name provded! Specify output name like so: makeicon ... outputname.ico");

    // The maximum size allows in an ICO file is 256x256! We also check for 0 or less as that would not be valid either...
    for(auto& size: options.sizes)
    {
        if(size > 256)
            ERROR("Invalid icon size '%d'! Maximum value allowed is 256 pixels.", size);
        if(size <= 0)
            ERROR("Invalid icon size '%d'! Minimum value allowed is 1 pixel.", size);
    }

    // Sort the input images from largest to smallest.
    std::sort(options.input.begin(), options.input.end());

    // Takes the populated options structure and uses those options to generate an icon for the desired platform.
    return make_icon(options);
}

//
// Windows
//

// Windows ICO File Format: https://en.wikipedia.org/wiki/ICO_(file_format)#Outline

typedef u16 ImageType;
enum ImageType_
{
    ImageType_Ico = 1,
    ImageType_Cur = 2
};

#pragma pack(push,1)
struct IconDir
{
    u16 reserved;
    u16 type;
    u16 num_images;
};
#pragma pack(pop)

#pragma pack(push,1)
struct IconDirEntry
{
    u8  width;
    u8  height;
    u8  num_colors;
    u8  reserved;
    u16 color_planes;
    u16 bpp; // bits per pixel
    u32 size;
    u32 offset;
};
#pragma pack(pop)

s32 make_icon_win32(const Options& options, const std::vector<Image>& input_images)
{
    std::vector<PngImage> output_images;

    for (auto size : options.sizes)
    {
        // find image of matching size
        auto it = std::find_if(input_images.begin(), input_images.end(), [=](const Image& image) { return image.width == size && image.height == size; });

        if (it != input_images.end())
        {
            output_images.push_back(PngImage(*it));
        }
        else
        {
            // did not find matching input image, so we have to create it
            Image resized;
            resize_image(input_images.back(), size, size, resized);
            output_images.push_back(PngImage(resized));
            free_image(resized);
        }
    }

    // Header
    IconDir icon_header;
    icon_header.reserved = 0;
    icon_header.type = ImageType_Ico;
    icon_header.num_images = CAST(u16, options.sizes.size());

    // Directory
    size_t offset = sizeof(IconDir) + (sizeof(IconDirEntry) * options.sizes.size());
    std::vector<IconDirEntry> icon_directory;
    for(const auto& image: output_images)
    {
        IconDirEntry icon_dir_entry;
        icon_dir_entry.width = CAST(u8, image.width); // Values of 256 (the max) will turn into 0 on cast, which is what the ICO spec wants.
        icon_dir_entry.height = CAST(u8, image.height);
        icon_dir_entry.num_colors = 0;
        icon_dir_entry.reserved = 0;
        icon_dir_entry.color_planes = 0;
        icon_dir_entry.bpp = 4*8; // We force to 4-channel RGBA!
        icon_dir_entry.size = CAST(u32, image.data_size);
        icon_dir_entry.offset = CAST(u32, offset);
        icon_directory.push_back(icon_dir_entry);
        offset += icon_dir_entry.size;
    }

    // Save
    std::ofstream output(options.output, std::ios::binary|std::ios::trunc);
    if(!output.is_open())
    {
        ERROR("Failed to save output file: %s", options.output.c_str());
    }
    else
    {
        output.write((CAST(char*, &icon_header)), sizeof(icon_header));
        for(auto& dir_entry: icon_directory)
        {
            output.write((CAST(char*, &dir_entry)), sizeof(dir_entry));
        }
        for(const auto& image: output_images)
        {
            output.write(CAST(char*, image.data), image.data_size);
        }
    }

    for (auto& image : output_images)
    {
        free_png_image(image);
    }

    return EXIT_SUCCESS;
}

//
// Android
//

s32 make_icon_android(const Options& options, const std::vector<Image>& input_images)
{
    // Android needs specific downsampled sizes for thumbnails.

    s32 sizes[5];
    sizes[0] = options.sizes[0];
    sizes[1] = (sizes[0] / 2) + (sizes[0] / 4);
    sizes[2] = (sizes[0] / 2);
    sizes[3] = (sizes[0] / 4) + (sizes[0] / 8);
    sizes[4] = (sizes[0] / 4);

    const char* directories[5] =
    {
        "mipmap-xxxhdpi/",
        "mipmap-xxhdpi/",
        "mipmap-xhdpi/",
        "mipmap-hdpi/",
        "mipmap-mdpi/"
    };

    // Create output directory.
    std::filesystem::path output_directory = options.output;
    if(!std::filesystem::exists(output_directory))
    {
        std::filesystem::create_directory(output_directory);
    }

    for(s32 i=0; i<5; ++i)
    {
        std::filesystem::path directory = options.output + "/" + directories[i];
        if(!std::filesystem::exists(directory))
        {
            std::filesystem::create_directory(directory);
        }
        resize_and_save_image(directory.string() + "ic_launcher.png", input_images, sizes[i], options.resize);
    }

    return EXIT_SUCCESS;
}

//
// Apple
//

s32 make_icon_apple(const Options& options, const std::vector<Image>& input_images)
{
    if(options.contents.empty())
    {
        ERROR("No contents json file specified! Specify contents file using: -sizes:Contents.json...");
    }

    // Read in JSON contents file that specifies the required output images.
    FILE* file = fopen(options.contents.c_str(), "rb");
    if(!file) ERROR("Failed to open contents file!");

    fseek(file, 0L, SEEK_END);
    size_t len = CAST(size_t,ftell(file));

    fseek(file, 0L, SEEK_SET);

    char* buf = CAST(char*, malloc((len+1)*sizeof(char)));
    if(!buf) ERROR("Failed to allocate file buffer for JSON!");

    buf[len] = '\0';

    fread(buf, 1, len, file);

    fclose(file);

    std::stringstream ss(buf);
    std::string to;

    // Create output directory.
    std::filesystem::path output_directory = options.output;
    if(!std::filesystem::exists(output_directory))
    {
        std::filesystem::create_directory(output_directory);
    }

    // Iterate over the lines of json and find parameters for resizing and saving the images.
    std::string filename = "";
    f32 scale = 0;
    f32 size = 0;
    while(getline(ss, to, '\n'))
    {
        s32 start = CAST(s32, to.find(": \"") + 3);

        if(to.find("filename") != -1)
        {
            s32 end = CAST(s32, to.find("\","));
            filename = to.substr(start, (end - start));
        }
        else if(to.find("scale") != -1)
        {
            s32 end = CAST(s32, to.find("x"));
            scale = std::stof(to.substr(start, (end - start)));
        }
        else if(to.find("size") != -1)
        {
            s32 end = CAST(s32, to.find("x"));
            size = std::stof(to.substr(start, (end - start)));
        }

        // Erase all stored parameters on hitting the end of the json object.
        if(to.find('}') != -1)
        {
            filename = "";
            size = 0;
            scale = 0;
        }

        // Once all parameters are filled write out an image and reset.
        if(!filename.empty() && scale && size)
        {
            resize_and_save_image(options.output + "/" + filename, input_images, CAST(s32, size * scale), options.resize);

            filename = "";
            scale = 0;
            size = 0;
        }
    }

    free(buf);

    // Copy the contents file to the output directory so all data is packaged together.
    std::string outputContentsPath = options.output + "/Contents.json";
    if(options.contents != outputContentsPath)
        std::filesystem::copy(options.contents, outputContentsPath);

    return EXIT_SUCCESS;
}
