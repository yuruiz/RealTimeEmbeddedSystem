/*
 * 18-648
 * Team 6
 *
 * header for energy tracking
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/cpufreq.h>
#include <linux/energy.h>
#include <asm/uaccess.h>
#include "energy_struct.h"

struct kobject *tasks_kobj = NULL;

/* 0 for not tracing energy, 1 for tracking */
int energy_tracking = 0;

/* total energy consumed by the system (muJ) */
unsigned long total_energy = 0;

/* spin lock for total_energy */
spinlock_t total_energy_lock;

/* 0 for not initializing spin lock yet, 1 for already initializing */
int lock_initialized = 0;

/* lookup table from cpu frequency (MHz) to power consumption (uW) */
unsigned int freq_to_power[] = {
28860, 28964, 29069, 29175, 29283, 29391, 29502, 29613, 29726, 29840, 
29955, 30072, 30190, 30309, 30429, 30551, 30674, 30798, 30923, 31050, 
31177, 31306, 31437, 31568, 31701, 31834, 31969, 32105, 32243, 32381, 
32521, 32662, 32804, 32947, 33091, 33236, 33383, 33531, 33679, 33829, 
33980, 34132, 34286, 34440, 34596, 34752, 34910, 35069, 35228, 35389, 
35551, 35715, 35879, 36044, 36210, 36378, 36546, 36716, 36886, 37058, 
37230, 37404, 37579, 37755, 37932, 38109, 38288, 38468, 38649, 38831, 
39014, 39198, 39383, 39569, 39756, 39944, 40133, 40323, 40514, 40706, 
40899, 41093, 41288, 41484, 41681, 41879, 42078, 42278, 42479, 42681, 
42883, 43087, 43292, 43498, 43704, 43912, 44121, 44330, 44541, 44752, 
44964, 45178, 45392, 45607, 45823, 46040, 46258, 46477, 46697, 46918, 
47140, 47362, 47586, 47810, 48036, 48262, 48489, 48717, 48946, 49176, 
49407, 49639, 49872, 50105, 50340, 50575, 50812, 51049, 51287, 51526, 
51766, 52007, 52248, 52491, 52734, 52978, 53224, 53470, 53717, 53964, 
54213, 54463, 54713, 54964, 55217, 55470, 55724, 55978, 56234, 56491, 
56748, 57006, 57265, 57525, 57786, 58048, 58310, 58573, 58838, 59103, 
59369, 59635, 59903, 60171, 60441, 60711, 60982, 61254, 61526, 61800, 
62074, 62349, 62625, 62902, 63179, 63458, 63737, 64017, 64298, 64580, 
64863, 65146, 65430, 65715, 66001, 66288, 66575, 66863, 67153, 67442, 
67733, 68025, 68317, 68610, 68904, 69199, 69495, 69791, 70088, 70386, 
70685, 70984, 71285, 71586, 71888, 72191, 72494, 72798, 73104, 73410, 
73716, 74024, 74332, 74641, 74951, 75262, 75573, 75885, 76198, 76512, 
76827, 77142, 77458, 77775, 78093, 78411, 78730, 79050, 79371, 79692, 
80015, 80338, 80662, 80986, 81312, 81638, 81965, 82292, 82621, 82950, 
83280, 83611, 83942, 84274, 84607, 84941, 85276, 85611, 85947, 86284, 
86621, 86959, 87298, 87638, 87979, 88320, 88662, 89005, 89348, 89692, 
90037, 90383, 90730, 91077, 91425, 91774, 92123, 92473, 92824, 93176, 
93528, 93881, 94235, 94590, 94945, 95301, 95658, 96016, 96374, 96733, 
97093, 97453, 97814, 98176, 98539, 98902, 99266, 99631, 99997, 100363, 
100730, 101098, 101466, 101835, 102205, 102576, 102947, 103319, 103692, 104065, 
104440, 104814, 105190, 105566, 105943, 106321, 106700, 107079, 107459, 107839, 
108221, 108603, 108985, 109369, 109753, 110138, 110523, 110909, 111296, 111684, 
112072, 112461, 112851, 113242, 113633, 114025, 114417, 114810, 115204, 115599, 
115994, 116390, 116787, 117185, 117583, 117982, 118381, 118781, 119182, 119584, 
119986, 120389, 120793, 121197, 121602, 122008, 122414, 122821, 123229, 123637, 
124047, 124456, 124867, 125278, 125690, 126103, 126516, 126930, 127344, 127760, 
128176, 128592, 129010, 129428, 129846, 130266, 130686, 131106, 131528, 131950, 
132373, 132796, 133220, 133645, 134070, 134496, 134923, 135350, 135779, 136207, 
136637, 137067, 137498, 137929, 138361, 138794, 139227, 139662, 140096, 140532, 
140968, 141405, 141842, 142280, 142719, 143158, 143598, 144039, 144480, 144922, 
145365, 145809, 146253, 146697, 147143, 147589, 148035, 148483, 148930, 149379, 
149828, 150278, 150729, 151180, 151632, 152084, 152538, 152991, 153446, 153901, 
154357, 154813, 155270, 155728, 156186, 156645, 157105, 157565, 158026, 158488, 
158950, 159413, 159877, 160341, 160806, 161271, 161737, 162204, 162671, 163139, 
163608, 164077, 164547, 165018, 165489, 165961, 166434, 166907, 167380, 167855, 
168330, 168806, 169282, 169759, 170236, 170715, 171194, 171673, 172153, 172634, 
173115, 173597, 174080, 174563, 175047, 175532, 176017, 176503, 176989, 177476, 
177964, 178452, 178941, 179431, 179921, 180412, 180903, 181395, 181888, 182381, 
182875, 183370, 183865, 184361, 184857, 185354, 185852, 186350, 186849, 187349, 
187849, 188350, 188851, 189353, 189856, 190359, 190863, 191367, 191872, 192378, 
192885, 193391, 193899, 194407, 194916, 195425, 195936, 196446, 196957, 197469, 
197982, 198495, 199009, 199523, 200038, 200553, 201070, 201586, 202104, 202622, 
203140, 203660, 204179, 204700, 205221, 205742, 206265, 206788, 207311, 207835, 
208360, 208885, 209411, 209937, 210464, 210992, 211520, 212049, 212579, 213109, 
213640, 214171, 214703, 215235, 215769, 216302, 216837, 217371, 217907, 218443, 
218980, 219517, 220055, 220594, 221133, 221672, 222213, 222754, 223295, 223837, 
224380, 224923, 225467, 226011, 226556, 227102, 227648, 228195, 228743, 229291, 
229839, 230389, 230938, 231489, 232040, 232591, 233144, 233696, 234250, 234804, 
235358, 235913, 236469, 237025, 237582, 238140, 238698, 239256, 239816, 240375, 
240936, 241497, 242058, 242621, 243183, 243747, 244311, 244875, 245440, 246006, 
246572, 247139, 247706, 248274, 248843, 249412, 249982, 250552, 251123, 251695, 
252267, 252839, 253413, 253986, 254561, 255136, 255711, 256287, 256864, 257441, 
258019, 258598, 259177, 259756, 260337, 260917, 261499, 262081, 262663, 263246, 
263830, 264414, 264999, 265584, 266170, 266756, 267343, 267931, 268519, 269108, 
269697, 270287, 270878, 271469, 272061, 272653, 273246, 273839, 274433, 275027, 
275622, 276218, 276814, 277411, 278008, 278606, 279205, 279804, 280403, 281004, 
281604, 282206, 282807, 283410, 284013, 284616, 285221, 285825, 286431, 287036, 
287643, 288250, 288857, 289465, 290074, 290683, 291293, 291903, 292514, 293126, 
293738, 294350, 294963, 295577, 296191, 296806, 297422, 298038, 298654, 299271, 
299889, 300507, 301126, 301745, 302365, 302985, 303606, 304228, 304850, 305472, 
306096, 306719, 307344, 307969, 308594, 309220, 309846, 310474, 311101, 311729, 
312358, 312987, 313617, 314248, 314879, 315510, 316142, 316775, 317408, 318042, 
318676, 319311, 319946, 320582, 321219, 321856, 322493, 323131, 323770, 324409, 
325049, 325689, 326330, 326972, 327614, 328256, 328899, 329543, 330187, 330832, 
331477, 332123, 332769, 333416, 334063, 334711, 335360, 336009, 336659, 337309, 
337960, 338611, 339263, 339915, 340568, 341221, 341875, 342530, 343185, 343840, 
344497, 345153, 345811, 346468, 347127, 347785, 348445, 349105, 349765, 350426, 
351088, 351750, 352413, 353076, 353740, 354404, 355069, 355734, 356400, 357066, 
357733, 358401, 359069, 359737, 360406, 361076, 361746, 362417, 363088, 363760, 
364432, 365105, 365779, 366452, 367127, 367802, 368477, 369154, 369830, 370507, 
371185, 371863, 372542, 373221, 373901, 374581, 375262, 375944, 376626, 377308, 
377991, 378675, 379359, 380043, 380728, 381414, 382100, 382787, 383474, 384162, 
384850, 385539, 386228, 386918, 387609, 388300, 388991, 389683, 390376, 391069, 
391762, 392457, 393151, 393846, 394542, 395238, 395935, 396632, 397330, 398029, 
398727, 399427, 400127, 400827, 401528, 402230, 402932, 403634, 404337, 405041, 
405745, 406450, 407155, 407860, 408567, 409273, 409981, 410688, 411397, 412106, 
412815, 413525, 414235, 414946, 415657, 416369, 417082, 417795, 418508, 419222, 
419937, 420652, 421368, 422084, 422800, 423518, 424235, 424953, 425672, 426391, 
427111, 427831, 428552, 429273, 429995, 430718, 431440, 432164, 432888, 433612, 
434337, 435062, 435788, 436515, 437242, 437969, 438697, 439426, 440155, 440884, 
441615, 442345, 443076, 443808, 444540, 445273, 446006, 446739, 447474, 448208, 
448944, 449679, 450415, 451152, 451890, 452627, 453366, 454104, 454844, 455583, 
456324, 457065, 457806, 458548, 459290, 460033, 460777, 461520, 462265, 463010, 
463755, 464501, 465248, 465995, 466742, 467490, 468238, 468987, 469737, 470487, 
471238, 471989, 472740, 473492, 474245, 474998, 475751, 476505, 477260, 478015, 
478771, 479527, 480283, 481040, 481798, 482556, 483315, 484074, 484833, 485594, 
486354, 487115, 487877, 488639, 489402, 490165, 490929, 491693, 492457, 493223, 
493988, 494755, 495521, 496288, 497056, 497824, 498593, 499362, 500132, 500902, 
501673, 502444, 503216, 503988, 504760, 505534, 506307, 507082, 507856, 508631, 
509407, 510183, 510960, 511737, 512515, 513293, 514072, 514851, 515631, 516411, 
517192, 517973, 518754, 519537, 520319, 521102, 521886, 522670, 523455, 524240, 
525026, 525812, 526598, 527386, 528173, 528961, 529750, 530539, 531329, 532119, 
532910, 533701, 534492, 535284, 536077, 536870, 537664, 538458, 539252, 540047, 
540843, 541639, 542435, 543232, 544030, 544828, 545626, 546425, 547225, 548025, 
548825, 549626, 550428, 551230, 552032, 552835, 553639, 554442, 555247, 556052, 
556857, 557663, 558469, 559276, 560084, 560891, 561700, 562509, 563318, 564128, 
564938, 565749, 566560, 567372, 568184, 568997, 569810, 570624, 571438, 572252, 
573068, 573883, 574699, 575516, 576333, 577151, 577969, 578787, 579606, 580426, 
581246, 582067, 582888, 583709, 584531, 585353, 586176, 587000, 587824, 588648, 
589473, 590298, 591124, 591951, 592777, 593605, 594433, 595261, 596090, 596919, 
597749, 598579, 599409, 600241, 601072, 601904, 602737, 603570, 604404, 605238, 
606072, 606907, 607743, 608579, 609415, 610252, 611090, 611928, 612766, 613605, 
614444, 615284, 616124, 616965, 617806, 618648, 619490, 620333, 621176, 622020, 
622864, 623709, 624554, 625400, 626246, 627092, 627939, 628787, 629635, 630483, 
631332, 632182, 633031, 633882, 634733, 635584, 636436, 637288, 638141, 638994, 
639848, 640702, 641557, 642412, 643267, 644123, 644980, 645837, 646695, 647553, 
648411, 649270, 650129, 650989, 651850, 652711, 653572, 654434, 655296, 656159, 
657022, 657886, 658750, 659614, 660479, 661345, 662211, 663078, 663945, 664812, 
665680, 666548, 667417, 668287, 669157, 670027, 670898, 671769, 672641, 673513, 
674386, 675259, 676132, 677006, 677881, 678756, 679631, 680507, 681384, 682261, 
683138, 684016, 684894, 685773, 686652, 687532, 688412, 689293, 690174, 691056, 
691938, 692820, 693703, 694587, 695471, 696355, 697240, 698125, 699011, 699897, 
700784, 701671, 702559, 703447, 704336, 705225, 706114, 707004, 707895, 708786, 
709677, 710569, 711462, 712354, 713248, 714141, 715036, 715930, 716825, 717721, 
718617, 719514, 720411, 721308, 722206, 723105, 724003, 724903, 725803, 726703};


ssize_t energy_show(struct kobject *kobj, _energy_attribute_t *attr, char *buf) {
    return scnprintf(buf, PAGE_SIZE, "%lu\n", attr->task->reserve.energy / MICRO_W_PER_MILLI_W);
}


SYSCALL_DEFINE2(get_energy_info, pid_t, tid, struct energy_struct*, energy_info) {
    struct task_struct *task;
    unsigned int freq, power;
    int cpu;
    unsigned long retval, energy;

    if (tid < 0) return EINVAL;
    if (tid && (task = find_task_by_vpid(tid)) == NULL) return ESRCH;

    if (tid) {
        freq = cpufreq_get(task->reserve.cpu) / KHZ_PER_MHZ;
        if (freq >= MIN_FREQ && freq <= MAX_FREQ) {
            power = freq_to_power[freq - MIN_FREQ] / MICRO_J_PER_MILLI_J;
        }
        else {
            power = 0;
        }
        energy = task->reserve.energy / MICRO_J_PER_MILLI_J;
    }
    /* energy information for the system  */
    else {
        for_each_online_cpu(cpu) {
            freq = cpufreq_get(cpu) / KHZ_PER_MHZ;
            break;
        }
        power = freq_to_power[freq - MIN_FREQ] * num_online_cpus() / MICRO_J_PER_MILLI_J;
        energy = total_energy / MICRO_J_PER_MILLI_J;
    }

    retval = copy_to_user(&(energy_info->freq), &freq, sizeof(unsigned int));
    if (retval) return retval;

    retval = copy_to_user(&(energy_info->power), &power, sizeof(unsigned int));
    if (retval) return retval;

    retval = copy_to_user(&(energy_info->energy), &(energy), sizeof(unsigned long));
    if (retval) return retval;

    return 0;
}
