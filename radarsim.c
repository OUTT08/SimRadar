//
//  radarsim.c
//  Radar Simulator
//
//  This is an example of how to use the Radar Simulator framework (librs.a).
//  All functions with prefix RS_ are built-in functions in the framework
//  and this example shows the general flow of using the framework to emulate.
//  a radar and generate time-series data. The RS framework is written in C
//  so any superset language, e.g., C++, Objective-C, and Visual C++, can use
//  the framework to generate radar data.
//
//  Framework requirements: OpenCL 1.1
//
//  Created by Boon Leng Cheong.
//
//

#include "rs.h"
#include "iq.h"
#include <getopt.h>

#if defined (_OPEN_MPI)
#include <mpi.h>
#endif

enum ACCEL_TYPE {
    ACCEL_TYPE_GPU,
    ACCEL_TYPE_CPU
};

enum SCAN_MODE {
    SCAN_MODE_STARE,
    SCAN_MODE_PPI,
    SCAN_MODE_RHI
};

typedef struct scan_params {
    char mode;
    float start;
    float end;
    float delta;
    float az;
    float el;
} ScanParams;

typedef struct user_params {
    float beamwidth;
    float density;
    float lambda;
    float prt;
    float pw;

    int   num_pulses;
    int   warm_up_pulses;
    int   seed;
    
    char output_iq_file;
    char output_state_file;
    char preview_only;
    char quiet_mode;
    char skip_questions;
    char tight_box;
    char show_progress;
    
    char output_dir[1024];
} UserParams;

typedef union simstate {
    char raw[63 * 1024];
    RSHandle master;
} SimState;

int get_next_scan_angles(ScanParams *params) {
    if (params->mode == SCAN_MODE_PPI) {
        params->az += params->delta;
        if (params->delta > 0.0f) {
            if (params->az > params->end) {
                params->az = params->start;
            } else if (params->az < params->start) {
                params->az = params->end;
            }
        } else {
            if (params->az > params->start) {
                params->az = params->end;
            } else if (params->az < params->end) {
                params->az = params->start;
            }

        }
        if (params->az < params->start || params->az > params->end) {
            return 1;
        }
    } else if (params->mode == SCAN_MODE_RHI) {
        params->el += params->delta;
        if (params->el > params->end) {
            params->el = params->start;
        } else if (params->el < params->start) {
            params->el = params->end;
        }
        if (params->el < params->start || params->el > params->end) {
            return 2;
        }
    }
    return 0;
}

static char *scan_mode_str(char scan_mode) {
    static char str[16];
    switch (scan_mode) {
        case SCAN_MODE_PPI:
        snprintf(str, sizeof(str), "PPI");
        break;
        
        case SCAN_MODE_RHI:
        snprintf(str, sizeof(str), "RHI");
        break;

        case SCAN_MODE_STARE:
        snprintf(str, sizeof(str), "STARE");
        break;

        default:
        break;
    }
    return str;
}

//
//   s h o w _ h e l p
//
#define CLEAR         "\033[0m"
#define UNDERLINE(x)  "\033[4m" x "\033[24m"
#define PROGNAME      "radarsim"

void show_help() {
    printf("Radar simulation\n\n"
           PROGNAME " [options]\n\n"
           "OPTIONS\n"
           "     Unless specifically stated, all options are interpreted in sequence. Some\n"
           "     options can be specified multiples times for repetitions. For example, the\n"
           "     debris particle count is set for each type sequentially by repeating the\n"
           "     option multiple times for each debris type.\n"
           "\n"
           "  -a (--azimuth) " UNDERLINE("angle") "\n"
           "         Sets the scan azimuth to " UNDERLINE("angle") " degrees.\n"
           "         See --sweep for more information.\n"
           "\n"
           "  --alarm\n"
           "         Make an alarm when the simulation is complete.\n"
           "\n"
           "  -c (--concept) " UNDERLINE("concepts") "\n"
           "         Sets the simulation concepts to be used, which are OR together for\n"
           "         multiple values that can be combined together.\n"
           "            D - Dragged background.\n"
           "            U - Uniform rain drop size density with scaled RCS.\n"
           "            V - Bounded particle velocity.\n"
           "         Examples:\n"
           "            --concept DU\n"
           "                sets simulation to use the concept of dragged background and\n"
           "                uniform density of rain drop size.\n"
           "            --concept V\n"
           "                sets simulation to use the concept of bounded particle velocity\n"
           "                but left the others as default.\n"
           "\n"
           "  -d (--debris) " UNDERLINE("count") "\n"
           "         Sets the population of debris to " UNDERLINE("count") ".\n"
           "         When is option is specified multiple times, multiple debris types will\n"
           "         be used in the simulator.\n"
           "         Debris type is as follows:\n"
           "            o  Leaf\n"
           "            o  Wood Board\n"
           "\n"
           "  -D (--density) " UNDERLINE("D") "\n"
           "         Set the density of particles to " UNDERLINE("D") " scatterers per resolution volume\n"
           "\n"
           "  --dontask\n"
           "         Sets the program to skip all the confirmation questions.\n"
           "\n"
           "  -e (--elevation) " UNDERLINE("angle") "\n"
           "         Sets the scan elevation to " UNDERLINE("angle") " degrees.\n"
           "         See --sweep for more information.\n"
           "\n"
           "  -f " UNDERLINE("count") "\n"
           "         Sets the number of frames to " UNDERLINE("count") ". This option is identical -p.\n"
           "         See -p for more information.\n"
           "\n"
           "  --lambda " UNDERLINE("wavelength") "\n"
           "         Sets the radar wavelength to " UNDERLINE("wavelength") " meters. Framework default value\n"
           "         is 10 cm.\n"
           "\n"
           "  -N (--preview)\n"
           "         No simulation. Previews the scanning angles of the setup. No data will\n"
           "         be generated.\n"
           "\n"
           "  -o     Sets the program to produce an output file. The filename is derived\n"
           "         based on the current date and time and an output file with name like\n"
           "         sim-20160229-143941-E03.0.iq will be placed in the ~/Downloads folder.\n"
           "\n"
           "  -p " UNDERLINE("count") "\n"
           "         Sets the number of pulses to " UNDERLINE("count") ". There is no\n"
           "         hard boundaries on which pulse marks the end of a sweep. If user wants\n"
           "         a sweep that contain 2400 pulses, it can be accomplished by setting\n"
           "         sweep mode = P, start = -12, end = +12, delta = 0.01, and combine with\n"
           "         option -p 2400 for a simulation session of 2400 pulses.\n"
           "\n"
           "  --savestate\n"
           "         Sets the program to generate a simulation state file at the end of the\n"
           "         simulation. An output file like sim-20160229-143941-E03.0.simstate will\n"
           "         be generated in the ~/Downloads folder.\n"
           "\n"
           "  --sweep " UNDERLINE("M:S:E:D") "\n"
           "         Sets the beam to scan mode.\n"
           "         The argument " UNDERLINE("M:S:E:D") " are parameters for mode, start, end, and delta.\n"
           "            M = P for PPI (plane position indicator) mode\n"
           "            M = R for RHI (range height indicator) mode\n"
           "         Examples:\n"
           "            --sweep P:-12:12:0.01\n"
           "                sets the scan mode in PPI, start from azimuth -12-deg and ends\n"
           "                at azimuth +12-deg. The beam. Position delta is 0.01-deg, which\n"
           "                means the azimuth changes by 0.01-deg at every pulse.\n"
           "            --sweep R:0.5:12.0:0.02\n"
           "                sets the scan mode in RHI, start from elevation 0.5-deg and ends\n"
           "                at elevation 12.0-deg. The beam position delta is 0.02-deg, which\n"
           "                means the elevation changes by 0.02-deg at every pulse.\n"
           "\n"
           "  -t " UNDERLINE("period") "\n"
           "         Sets the pulse repetition time to " UNDERLINE("period") " seconds.\n"
           "\n"
           "  -T (--tightbox)\n"
           "         Sets the program to use a tight box, i.e., only simulate from ground to\n"
           "         the scan elevation. Note that framework padding will still be respected.\n"
           "\n"
           "  -W (--warmup) " UNDERLINE("count") "\n"
           "         Sets the warm up stage to use " UNDERLINE("count") " pulses.\n"
           "\n\n"
           "EXAMPLES\n"
           "     The following simulates a vortex and creates a PPI scan data using default\n"
           "     scan parameters. This allows you quickly check if the tools works. An\n"
           "     output file will be generated in the ~/Downloads folder.\n"
           "           " PROGNAME " -o\n"
           "\n"
           "     The following simulates a vortex and creates a PPI scan data using\n"
           "     scan parameter: mode = 'P' (PPI), start = -12, end = +12, delta = 0.01,\n"
           "     el = 3.0 deg, p = 2400 (number of pulses).\n"
           "           " PROGNAME " -e 3.0 --sweep P:-12:12:0.01 -p 2400 -o\n"
           "\n"
           "     The following simulates a vortex and creates an RHI scan data using\n"
           "     scan parameters: mode = 'R' (RHI), start = 0, end = 12, delta = 0.01,\n"
           "     az = 1.0 deg, p = 1200.\n"
           "           " PROGNAME " -a 1.0 --sweep R:0:12:0.01 -p 1200 -o\n"
           "\n"
           "     The following simulates a vortex and creates a PPI scan data using\n"
           "     10,000 debris type #1, which is the leaf.\n"
           "           " PROGNAME " -a 1.0 --sweep R:0:12:0.01 -p 1200 -d 10000 -o\n"
           "\n"
           "     The following simulates a vortex and creates a PPI scan data with\n"
           "     scan parameters: mode = 'P' (PPI), start = -12, end = +12, delta = 0.01,\n"
           "     el = 3.0 (not set, default). With PRT = 5ms, it takes 4800 pulses to cover\n"
           "     the sector so -p 4800 would complete the sweep.\n"
           "           " PROGNAME " -T --sweep P:-12:12:0.005 -t 0.0005 -o -p 4800\n"
           );
}

enum ValueType {
    ValueTypeInt,
    ValueTypeFloat,
    ValueTypeChar,
    ValueTypeNotSupplied
};

#define PARAMS_FLOAT_NOT_SUPPLIED   -999.9f
#define PARAMS_INT_NOT_SUPPLIED     -999

void show_user_param(const char *name, const void* value, const char *unit, char type) {
    char str_buf[64] = "not supplied";
    char *value_str = str_buf;
    float *fp;
    int *ip;
    switch (type) {
        case ValueTypeInt:
            ip = (int *)value;
            if (*ip == PARAMS_INT_NOT_SUPPLIED) {
                value_str = str_buf;
            } else {
                value_str = commaint(*ip);
            }
            break;
        case ValueTypeFloat:
            fp = (float *)value;
            if (*fp == PARAMS_FLOAT_NOT_SUPPLIED) {
                value_str = str_buf;
            } else {
                value_str = commafloat(*fp);
            }
            break;
        case ValueTypeChar:
            value_str = (char *)value;
            if (strlen(value_str) == 0) {
                value_str = str_buf;
            }
            break;
        default:
            value_str = str_buf;
            break;
    }
    printf("  %-25s = %s %s\n", name, value_str, value_str == str_buf ? "" : unit);
}

void write_iq_file(const UserParams user, const ScanParams scan, const IQFileHeader *file_header, const IQPulseHeader *pulse_headers, const cl_float4 *pulse_cache, const int stride) {
    char charbuff[2048];
    
    memset(charbuff, 0, sizeof(charbuff));
    snprintf(charbuff, sizeof(charbuff), "%s/sim-%s-%s%04.1f.iq",
             user.output_dir,
             nowlong(),
             scan.mode == SCAN_MODE_PPI ? "E": (scan.mode == SCAN_MODE_RHI ? "A" : "S"),
             scan.mode == SCAN_MODE_PPI ? scan.el: (scan.mode == SCAN_MODE_RHI ? scan.az : (float)user.num_pulses));
    printf("%s : Output file : " UNDERLINE("%s") "\n", now(), charbuff);
    FILE *fid = fopen(charbuff, "wb");
    if (fid == NULL) {
        fprintf(stderr, "%s : Error creating file for IQ data.\n", now());
    }
    fwrite(file_header, sizeof(IQFileHeader), 1, fid);
    
    // Flush out the cache
    for (int k = 0; k < user.num_pulses; k++) {
        fwrite(&pulse_headers[k], sizeof(IQPulseHeader), 1, fid);
        fwrite(&pulse_cache[k * stride], sizeof(cl_float4), stride, fid);
    }
    printf("%s : Data file with %s B.\n", now(), commaint(ftell(fid)));
    fclose(fid);
}

//
//
//  M A I N
//
//
int main(int argc, char *argv[]) {
    
    int k = 0;
    char verb = 0;
    char accel_type = 0;
    char charbuff[4096];
    FILE *fid = NULL;

    // A structure unit that encapsulates command line user parameters
    UserParams user;
    user.beamwidth         = PARAMS_FLOAT_NOT_SUPPLIED;
    user.density           = PARAMS_FLOAT_NOT_SUPPLIED;
    user.lambda            = PARAMS_FLOAT_NOT_SUPPLIED;
    user.prt               = PARAMS_FLOAT_NOT_SUPPLIED;
    user.pw                = PARAMS_FLOAT_NOT_SUPPLIED;

    user.seed              = PARAMS_INT_NOT_SUPPLIED;
    user.num_pulses        = PARAMS_INT_NOT_SUPPLIED;
    user.warm_up_pulses    = PARAMS_INT_NOT_SUPPLIED;

    user.output_iq_file    = false;
    user.output_state_file = false;
    user.preview_only      = false;
    user.quiet_mode        = true;
    user.skip_questions    = false;
    user.show_progress     = true;
    user.tight_box         = false;
    
    user.output_dir[0]     = '\0';

    // A structure unit that encapsulates the scan strategy
    ScanParams scan;
    scan.mode     = SCAN_MODE_PPI;
    scan.start    = - 12.0f;
    scan.end      = +12.0f;
    scan.delta    = 0.01f;
    scan.az       = scan.start;
    scan.el       = 3.0f;
    
    struct timeval t0, t1, t2;
    
    gettimeofday(&t0, NULL);
    
    int debris_types = 0;
    int debris_count[RS_MAX_DEBRIS_TYPES];
    RSSimulationConcept concept = RSSimulationConceptDraggedBackground
                                | RSSimulationConceptBoundedParticleVelocity
                                | RSSimulationConceptUniformDSDScaledRCS;
    
    memset(debris_count, 0, RS_MAX_DEBRIS_TYPES * sizeof(int));

    IQFileHeader file_header;
    memset(&file_header, 0, sizeof(IQFileHeader));
    
    // ---------------------------------------------------------------------------------------------------------------

    static struct option long_options[] = {
        {"alarm"      , no_argument      , 0, 'A'}, // ASCII 65 - 90 : A - Z
        {"cpu"        , no_argument      , 0, 'C'},
        {"density"    , required_argument, 0, 'D'},
        {"savestate"  , no_argument      , 0 ,'E'},
        {"noprogress" , no_argument      , 0 ,'F'},
        {"preview"    , no_argument      , 0, 'N'},
        {"outdir"     , required_argument, 0, 'O'},
        {"sweep"      , required_argument, 0, 'S'},
        {"tightbox"   , no_argument      , 0, 'T'},
        {"warmup"     , required_argument, 0, 'W'},
        {"azimuth"    , required_argument, 0, 'a'}, // ASCII 97 - 122 : a - z
        {"concept"    , required_argument, 0, 'c'},
        {"debris"     , required_argument, 0, 'd'},
        {"elevation"  , required_argument, 0, 'e'},
        {"help"       , no_argument      , 0, 'h'},
        {"gpu"        , no_argument      , 0, 'g'},
        {"frames"     , required_argument, 0, 'f'},
        {"lambda"     , required_argument, 0, 'l'},
        {"output"     , no_argument      , 0, 'o'},
        {"pulses"     , required_argument, 0, 'p'},
        {"seed"       , required_argument, 0, 's'},
        {"prt"        , required_argument, 0, 't'},
        {"pulsewidth" , required_argument, 0, 'w'},
        {"quiet"      , no_argument      , 0, 'q'},
        {"verbose"    , no_argument      , 0, 'v'},
        {"dontask"    , no_argument      , 0, 'y'},
        {0, 0, 0, 0}
    };
    
    // Construct short_options from long_options
    char str[1024] = "";
    for (k = 0; k < sizeof(long_options) / sizeof(struct option); k++) {
        struct option *o = &long_options[k];
        snprintf(str + strlen(str), 1024, "%c%s", o->val, o->has_arg ? ":" : "");
    }
    //printf("str = '%s'\n", str);
    
    char c1;
    float f1, f2, f3;
    // Process the input arguments and set the simulator parameters
    int opt, long_index = 0;
    while ((opt = getopt_long(argc, argv, str, long_options, &long_index)) != -1) {
        switch (opt) {
            case 'a':
                scan.az = atof(optarg);
                break;
            case 'A':
                user.quiet_mode = false;
                break;
            case 'c':
                concept = RSSimulationConceptNull;
                if (strcasestr(optarg, "B")) {
                    concept |= RSSimulationConceptBoundedParticleVelocity;
                }
                if (strcasestr(optarg, "D")) {
                    concept |= RSSimulationConceptDraggedBackground;
                }
                if (strcasestr(optarg, "U")) {
                    concept |= RSSimulationConceptUniformDSDScaledRCS;
                }
                break;
            case 'C':
                accel_type = ACCEL_TYPE_CPU;
                break;
            case 'd':
                debris_count[debris_types++] = atoi(optarg);
                break;
            case 'D':
                user.density = atof(optarg);
                break;
            case 'e':
                scan.el = atof(optarg);
                break;
            case 'E':
                user.output_state_file = true;
                break;
            case 'f':
                user.num_pulses = atoi(optarg);
                break;
            case 'F':
                user.show_progress = false;
                break;
            case 'g':
                accel_type = ACCEL_TYPE_GPU;
                break;
            case 'h':
                show_help();
                exit(EXIT_SUCCESS);
                break;
            case 'l':
                user.lambda = atof(optarg);
                break;
            case 'N':
                user.preview_only = true;
                break;
            case 'o':
                user.output_iq_file = true;
                break;
            case 'O':
                strncpy(user.output_dir, optarg, sizeof(user.output_dir));
                break;
            case 'p':
                user.num_pulses = atoi(optarg);
                break;
            case 'q':
                user.quiet_mode = true;
                break;
            case 's':
                user.seed = atoi(optarg);
                break;
            case 'S':
                k = sscanf(optarg, "%c:%f:%f:%f", &c1, &f1, &f2, &f3);
                if (k < 4) {
                    fprintf(stderr, "Error in scanmode argument.\n");
                    exit(EXIT_FAILURE);
                }
                scan.mode = c1 == 'P' ? SCAN_MODE_PPI : ( c1 == 'R' ? SCAN_MODE_RHI : SCAN_MODE_STARE);
                scan.start = f1;
                scan.end = f2;
                scan.delta = f3;
                if (scan.mode == SCAN_MODE_PPI) {
                    scan.az = f1;
                } else if (scan.mode == SCAN_MODE_RHI) {
                    scan.el = f1;
                }
                break;
            case 't':
                user.prt = atof(optarg);
                break;
            case 'T':
                user.tight_box = true;
                break;
            case 'v':
                verb++;
                break;
            case 'w':
                user.pw = atof(optarg);
                break;
            case 'W':
                user.warm_up_pulses = atoi(optarg);
                break;
            case 'y':
                user.skip_questions = true;
                break;
            default:
                exit(EXIT_FAILURE);
                break;
        }
    }

    
    // ---------------------------------------------------------------------------------------------------------------

#if defined (_OPEN_MPI)
    
    int world_size, world_rank;
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    
    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Get_processor_name(processor_name, &k);
    
    //printf("Node " UNDERLINE("%s") " ( %d out of %d )\n",  processor_name, world_rank, world_size);
    
    if (user.seed != PARAMS_INT_NOT_SUPPLIED) {
        user.seed += world_rank;
    }
    
#endif
    
    if (verb > 1) {
        printf("----------------------------------------------\n");
        printf("  User parameters:\n");
        printf("----------------------------------------------\n");
        show_user_param("Beamwidth", &user.beamwidth, "deg", ValueTypeFloat);
        show_user_param("TX lambda", &user.lambda, "m", ValueTypeFloat);
        show_user_param("TX pulse width", &user.pw, "s", ValueTypeFloat);
        show_user_param("Warm up pulses", &user.warm_up_pulses, "", ValueTypeInt);
        show_user_param("Number of pulses", &user.num_pulses, "", ValueTypeInt);
        show_user_param("Particle density", &user.density, "", ValueTypeFloat);
        show_user_param("Output directory", user.output_dir, "", ValueTypeChar);
        show_user_param("User random seed", &user.seed, "", ValueTypeInt);
        printf("----------------------------------------------\n");
    }

    // ---------------------------------------------------------------------------------------------------------------
    
    // Some conditions that no simulation should be commenced
    if (user.num_pulses < 0) {
        fprintf(stderr, "No pulses to be generated.\n");
        exit(EXIT_FAILURE);
    }
    
    if (sizeof(IQFileHeader) != sizeof(file_header.raw)) {
        fprintf(stderr, "Error. sizeof(IQFileHeader) = %zu  !=  sizeof(file_header.raw) = %zu\n", sizeof(IQFileHeader), sizeof(file_header.raw));
        exit(EXIT_FAILURE);
    }

    if (sizeof(RSHandle) > sizeof(SimState)) {
        fprintf(stderr, "Error. sizeof(RSHandler) = %zu  >  %s\n", sizeof(RSHandle), commaint(sizeof(SimState)));
        exit(EXIT_FAILURE);
    }

    // ---------------------------------------------------------------------------------------------------------------

    // Preview only
    if (user.preview_only) {
        #define FLT_FMT  "\033[1;33m%+6.2f\033[0m"
        printf("Scan mode: \033[1;32m%s\033[0m", scan_mode_str(scan.mode));
        if (scan.mode == SCAN_MODE_RHI) {
            printf("   AZ: " FLT_FMT " deg", scan.az);
        } else {
            printf("   EL: " FLT_FMT " deg", scan.el);
        }
        if (scan.mode == SCAN_MODE_RHI) {
            printf("   EL: " FLT_FMT " -- " FLT_FMT " deg    delta: " FLT_FMT " deg\n", scan.start, scan.end, scan.delta);
        } else if (scan.mode == SCAN_MODE_PPI) {
            printf("   AZ: " FLT_FMT " -- " FLT_FMT " deg    delta: " FLT_FMT " deg\n", scan.start, scan.end, scan.delta);
        } else {
            printf("   EL: " FLT_FMT " deg\n", scan.el);
        }
        for (k = 0; k < user.num_pulses; k++) {
            fprintf(stderr, "k = %4d   el = %6.2f deg   az = %5.2f deg\n", k, scan.el, scan.az);
            get_next_scan_angles(&scan);
        }
        return EXIT_SUCCESS;
    }

    // ---------------------------------------------------------------------------------------------------------------

    // Initialize the RS framework
    RSHandle *S;
    if (accel_type == ACCEL_TYPE_CPU) {
        S = RS_init_for_cpu_verbose(verb);
    } else {
        S = RS_init_verbose(verb);
    }
    if (S == NULL) {
        fprintf(stderr, "%s : Some errors occurred during RS_init().\n", now());
        return EXIT_FAILURE;
    }
    
#if defined (_OPEN_MPI)

    printf("%s : Session started on %s (%d / %d / %s)\n", now(), processor_name, world_rank, world_size, commaint(user.seed));

#else
    
    printf("%s : Session started\n", now());

#endif
    
    ADMHandle *A;
    LESHandle *L;
    RCSHandle *R;
    
    // Initialize the LES ingest
    L = LES_init();
    if (L == NULL) {
        fprintf(stderr, "%s : Some errors occurred during LES_init().\n", now());
        return EXIT_FAILURE;
    }
    
    // Initialize the ADM ingest
    A = ADM_init();
    if (A == NULL) {
        fprintf(stderr, "%s : Some errors occurred during ADM_init().\n", now());
        return EXIT_FAILURE;
    }
    
    // Initialize the RCS ingest
    R = RCS_init();
    if (R == NULL) {
        fprintf(stderr, "%s : Some errors occurred during RCS_init().\n", now());
        return EXIT_FAILURE;
    }
    
    RS_set_concept(S, concept);
    
    // ---------------------------------------------------------------------------------------------------------------
    
    // Pre-process some parameters to ensure proper logic
    
    if (user.num_pulses > 1000 && user.output_iq_file == false && user.skip_questions == false) {
        printf("Simulating more than 1,000 pulses but no file will be generated.\n"
               "Do you want to generate an output file instead (Y/N/N) ? ");
        c1 = getchar();
        if (c1 == 'y' || c1 == 'Y') {
            user.output_iq_file = true;
        }
    }
    
    if (user.warm_up_pulses == PARAMS_INT_NOT_SUPPLIED) {
        if (user.num_pulses > 1000)  {
            user.warm_up_pulses = 2000;
        } else {
            user.warm_up_pulses = 0;
        }
    }
    
    // ---------------------------------------------------------------------------------------------------------------
    
    // Set user parameters that were supplied
    if (user.beamwidth != PARAMS_FLOAT_NOT_SUPPLIED) {
        RS_set_antenna_params(S, user.beamwidth, 44.5f);
    }

    if (user.density != PARAMS_FLOAT_NOT_SUPPLIED) {
        RS_set_density(S, user.density);
    }
    
    if (user.pw != PARAMS_FLOAT_NOT_SUPPLIED) {
        RS_set_tx_params(S, user.pw, 50.0e3f);
    }
    
    if (user.lambda != PARAMS_FLOAT_NOT_SUPPLIED) {
        RS_set_lambda(S, user.lambda);
    }
    
    if (user.seed != PARAMS_INT_NOT_SUPPLIED) {
        RS_set_random_seed(S, user.seed);
    }

    // ---------------------------------------------------------------------------------------------------------------
    
#if defined (_OPEN_MPI)

    // Suppress child nodes output after this point for Open MPI runs
    
    if (world_rank > 0) {
        verb = 0;
        RS_set_verbosity(S, 0);
    }
    
#endif

    // Number of LES entries needed based on the number of pulses to be simulated
    int nvel = 0;
    if (user.prt == PARAMS_FLOAT_NOT_SUPPLIED) {
        nvel = (int)ceilf(((float)user.num_pulses * S->params.prt + (float)user.warm_up_pulses * 1.0f / 60.0f) / LES_get_table_period(L));
    } else {
        nvel = (int)ceilf(((float)user.num_pulses * user.prt + (float)user.warm_up_pulses * 1.0f / 60.0f) / LES_get_table_period(L));
    }
    if (nvel <= 0) {
        fprintf(stderr, "%s : nvel = 0.\n", now());
        exit(EXIT_FAILURE);
    }
    for (int k = 0; k < MIN(RS_MAX_VEL_TABLES, nvel); k++) {
        RS_set_vel_data_to_LES_table(S, LES_get_frame(L, k));
    }
    
    RS_set_adm_data_to_ADM_table(S, ADM_get_table(A, ADMConfigModelPlate));
    RS_set_adm_data_to_ADM_table(S, ADM_get_table(A, ADMConfigSquarePlate));
    
    RS_set_rcs_data_to_RCS_table(S, RCS_get_table(R, RCSConfigLeaf));
    RS_set_rcs_data_to_RCS_table(S, RCS_get_table(R, RCSConfigWoodBoard));

    RSBox box = RS_suggest_scan_domain(S, 16);
    
    // Set debris population
    for (k = 0; k < debris_types; k++) {
        if (debris_count[k]) {
            RS_set_debris_count(S, k + 1, debris_count[k]);
        }
    }
    RS_revise_debris_counts_to_gpu_preference(S);
    
    if (user.tight_box) {
        if (scan.mode == SCAN_MODE_PPI) {
            // No need to go all the way up if we are looking low
            box.size.e = MIN(box.size.e, scan.el);
        } else if (scan.mode == SCAN_MODE_RHI) {
            // Need to make sure we cover the very top
            box.size.e = MAX(scan.start, scan.end);
        }
    }
    
    RS_set_scan_box(S,
                    box.origin.r, box.origin.r + box.size.r, 15.0f,                       // Range
                    box.origin.a, box.origin.a + box.size.a, S->params.antenna_bw_deg,    // Azimuth
                    box.origin.e, box.origin.e + box.size.e, S->params.antenna_bw_deg);   // Elevation
    
    RS_set_dsd_to_mp(S);

    // Save the framework default PRT for later
    if (user.prt == PARAMS_FLOAT_NOT_SUPPLIED) {
        user.prt = S->params.prt;
    }

    if (verb) {
        RS_show_radar_params(S);
    }
    
    // Populate the domain with scatter bodies.
    // This is also the function that triggers kernel compilation, GPU memory allocation and
    // upload all the parameters to the GPU.
    RS_populate(S);
        
    // Show some basic info

#if defined (_OPEN_MPI)

    printf("%s : Emulating %s frame%s with %s scatter bodies on %s\n",
           now(), commaint(user.num_pulses), user.num_pulses > 1 ? "s" : "", commaint(S->num_scats), processor_name);

#else
    
    printf("%s : Emulating %s frame%s with %s scatter bodies\n",
           now(), commaint(user.num_pulses), user.num_pulses > 1 ? "s" : "", commaint(S->num_scats));
    
#endif

    // At this point, we are ready to bake
    float dt = 0.1f, fps = 0.0f, prog = 0.0f, eta = 9999999.0f;
    
    // Some warm up if we are going for real
    if (user.warm_up_pulses > 0) {
        RS_set_prt(S, 1.0f / 60.0f);
        gettimeofday(&t1, NULL);
        for (k = 0; k < user.warm_up_pulses; k++) {
            // Skip computing progress if we are not showing progress
            if (user.show_progress) {
                gettimeofday(&t2, NULL);
                dt = DTIME(t1, t2);
                if (dt >= 0.25f) {
                    t1 = t2;
                    fprintf(stderr, "Warming up ... %d out of %d ... \033[32m%.2f%%\033[0m  \r", k, user.warm_up_pulses, (float)k / user.warm_up_pulses * 100.0f);
                }
            }
            RS_advance_time(S);
        }
        if (user.show_progress) {
            fprintf(stderr, "%80s\r", " ");
        }
    }

    // Set PRT to the actual one
    RS_set_prt(S, user.prt);

    // ---------------------------------------------------------------------------------------------------------------
    
    gettimeofday(&t1, NULL);
    
    // Allocate a pulse cache
    IQPulseHeader *pulse_headers = (IQPulseHeader *)malloc(user.num_pulses * sizeof(IQPulseHeader));
    cl_float4 *pulse_cache = (cl_float4 *)malloc(user.num_pulses * S->params.range_count * sizeof(cl_float4));
    memset(pulse_headers, 0, user.num_pulses * sizeof(IQPulseHeader));
    memset(pulse_cache, 0, user.num_pulses * S->params.range_count * sizeof(cl_float4));

    // Now we bake
    int k0 = 0;
    for (k = 0; k < user.num_pulses; k++) {
        if (user.show_progress) {
            gettimeofday(&t2, NULL);
            dt = DTIME(t1, t2);
            if (dt >= 0.25f) {
                t1 = t2;
                prog =  (float)k / user.num_pulses * 100.0f;
                if (k > 3) {
                    fps = 0.5f * fps + 0.5f * (float)(k - k0) / dt;
                } else {
                    fps = (float)(k - k0) / dt;
                }
                eta = (float)(user.num_pulses - k) / fps;
                k0 = k;
                fprintf(stderr, "k %5d   (e%6.2f, a%5.2f)   %.2f fps   \033[1;33m%.2f%%\033[0m   eta %.0f second%s   \r", k, scan.el, scan.az, fps, prog, eta, eta > 1.5f ? "s" : "");
            }
        }
        RS_set_beam_pos(S, scan.az, scan.el);
        RS_make_pulse(S);
        RS_advance_time(S);

        // Only download the necessary data
        if (verb > 2) {
            RS_download(S);

            RS_show_scat_sig(S);
    
            printf("signal:\n");
            for (int r = 0; r < S->params.range_count; r++) {
                printf("sig[%d] = (%.4f %.4f %.4f %.4f)\n", r, S->pulse[r].s0, S->pulse[r].s1, S->pulse[r].s2, S->pulse[r].s3);
            }
            printf("\n");
        } else if (user.output_iq_file) {
            RS_download_pulse_only(S);
        }

        // Gather information for the  pulse header
        if (user.output_iq_file) {
            pulse_headers[k].time = S->sim_time;
            pulse_headers[k].az_deg = scan.az;
            pulse_headers[k].el_deg = scan.el;
            memcpy(&pulse_cache[k * S->params.range_count], S->pulse, S->params.range_count * sizeof(cl_float4));
        }

        // Update scan angles for the next pulse
        get_next_scan_angles(&scan);
    }

    // Overall fps
    gettimeofday(&t2, NULL);
    dt = DTIME(t0, t2);
    fps = user.num_pulses / dt;

    // Clear the last line and beep five times
    fprintf(stderr, "%120s\r", "");
    if (!user.quiet_mode) {
        #if defined (__APPLE__)
        system("say -v Bells dong dong dong dong &");
        #else
        fprintf(stderr, "\a\a\a\a\a");
        #endif
    }

#if defined (_OPEN_MPI)

    printf("%s : Finished on %s.  Total time elapsed = %.2f s  (%.1f FPS)\n", now(), processor_name, dt, fps);
    
#else
    
    printf("%s : Finished.  Total time elapsed = %.2f s  (%.1f FPS)\n", now(), dt, fps);

#endif
    
    // Download everything once we are all done.
    RS_download(S);

    if (verb > 2) {
        printf("%s : Final scatter body positions, velocities and orientations:\n", now());
        RS_show_scat_pos(S);
        RS_show_scat_sig(S);
    }

    // ---------------------------------------------------------------------------------------------------------------

    // Initialize a file if the user wants output files
    if (user.output_iq_file || user.output_state_file) {
        file_header.params = S->params;
        for (k = 0; k < S->num_body_types; k++) {
            file_header.debris_population[k] = (uint32_t)S->debris_population[k];
        }
        snprintf(file_header.scan_mode, sizeof(file_header.scan_mode), "%s", scan_mode_str(scan.mode));
        file_header.scan_start      = scan.start;
        file_header.scan_end        = scan.end;
        file_header.scan_delta      = scan.delta;
        file_header.simulation_seed = S->random_seed;
    }

    if (strlen(user.output_dir) == 0) {
        snprintf(user.output_dir, sizeof(user.output_dir), "%s/Downloads", getenv("HOME"));
    } else {
        size_t len = strlen(user.output_dir);
        if (user.output_dir[len - 1] == '\\') {
            user.output_dir[len - 1] = '\0';
        }
    }
    
    if (user.output_iq_file) {

#if defined (_OPEN_MPI)

        // Let master node do all the file writing to avoid identical filenames
        
        MPI_Status status;
        
        if (world_rank == 0) {
            
            write_iq_file(user, scan, &file_header, pulse_headers, pulse_cache, S->params.range_count);
            
            for (k = 1; k < world_size; k++) {
                MPI_Recv(&file_header, sizeof(file_header), MPI_BYTE, k, 0, MPI_COMM_WORLD, &status);
                printf("%s : Received header from node %d  (seed = %s).\n", now(), status.MPI_SOURCE, commaint(file_header.simulation_seed));
                MPI_Recv(pulse_headers, sizeof(pulse_headers), MPI_BYTE, k, 1, MPI_COMM_WORLD, &status);
                printf("%s : Received pulse headers from node %d.\n", now(), status.MPI_SOURCE);
                MPI_Recv(pulse_cache, sizeof(pulse_cache), MPI_BYTE, k, 2, MPI_COMM_WORLD, &status);
                printf("%s : Received pulse data from node %d.\n", now(), status.MPI_SOURCE);
                sleep(1);
                write_iq_file(user, scan, &file_header, pulse_headers, pulse_cache, S->params.range_count);
            }
        } else {
            //printf("%s : %s : Sending header to master node.\n", now(), processor_name);
            MPI_Send(&file_header, sizeof(file_header), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
            //printf("%s : %s : Sending pulse headers to master node.\n", now(), processor_name);
            MPI_Send(pulse_headers, sizeof(pulse_headers), MPI_BYTE, 0, 1, MPI_COMM_WORLD);
            //printf("%s : %s : Sending pulse data to master node.\n", now(), processor_name);
            MPI_Send(pulse_cache, sizeof(pulse_cache), MPI_BYTE, 0, 2, MPI_COMM_WORLD);
        }

#else
        
        write_iq_file(user, scan, &file_header, pulse_headers, pulse_cache, S->params.range_count);
        
#endif
        
    }
    
    if (user.output_state_file) {
        memset(charbuff, 0, sizeof(charbuff));
        snprintf(charbuff, sizeof(charbuff), "%s/sim-%s-%s%04.1f.simstate",
                 user.output_dir,
                 nowlong(),
                 scan.mode == SCAN_MODE_PPI ? "E": (scan.mode == SCAN_MODE_RHI ? "A" : "S"),
                 scan.mode == SCAN_MODE_PPI ? scan.el: (scan.mode == SCAN_MODE_RHI ? scan.az : (float)user.num_pulses));
        printf("%s : Output file : " UNDERLINE ("%s") "\n", now(), charbuff);
        fid = fopen(charbuff, "wb");
        if (fid == NULL) {
            fprintf(stderr, "%s : Error creating file for simulation state data.\n", now());
        }
        fwrite(&file_header, sizeof(IQFileHeader), 1, fid);
        SimState state;
        memset(&state, 0, sizeof(SimState));
        memcpy(&state.master, S, sizeof(RSHandle));
        fwrite(S, sizeof(SimState), 1, fid);
        if (verb > 1) {
            printf("%s : Total header size = %s B.\n", now(), commaint(ftell(fid)));
            printf("%s : sizeof(LESTable) = %zu   sizeof(ADMTable) = %zu   sizeof(RCSTable) = %zu\n", now(), sizeof(LESTable), sizeof(ADMTable), sizeof(RCSTable));
        }
        fwrite(S->scat_pos, sizeof(cl_float4), S->num_scats, fid);
        fwrite(S->scat_vel, sizeof(cl_float4), S->num_scats, fid);
        fwrite(S->scat_ori, sizeof(cl_float4), S->num_scats, fid);
        fwrite(S->scat_tum, sizeof(cl_float4), S->num_scats, fid);
        fwrite(S->scat_aux, sizeof(cl_float4), S->num_scats, fid);
        fwrite(S->scat_rcs, sizeof(cl_float4), S->num_scats, fid);
        fwrite(S->scat_sig, sizeof(cl_float4), S->num_scats, fid);
        fwrite(S->scat_rnd, sizeof(cl_uint4), S->num_scats, fid);
        printf("%s : State file with %s B.\n", now(), commaint(ftell(fid)));
        fclose(fid);
    }
    
    free(pulse_headers);
    free(pulse_cache);
    
    printf("%s : Session ended\n", now());

    RS_free(S);
    
    LES_free(L);
    
    ADM_free(A);
    
    RCS_free(R);
    
#if defined (_OPEN_MPI)

    MPI_Finalize();
    
#endif
    
    return EXIT_SUCCESS;
}
