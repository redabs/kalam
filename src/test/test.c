
int foo(float x, double y)  {
    return (int)(x * y);
}

typedef float f32;

typedef float *f32_ptr;

typedef void (*foo_func)(int, int);

typedef f32_ptr *f32_dbl_ptr;

typedef struct vector3_t {
    f32 x;
    f32 y;
    f32 z;
} vector3_t;

typedef struct bar_func_ret {int a; int b;} (bar_func)(int x, int y);

typedef vector3_t vec;

struct {
    int Id;
    vector3_t Vec3;
} typedef entity_t;

typedef enum {
    UP,
    DOWN,
    LEFT, 
    RIGHT
} direction_t;

struct mat3_t {
    vector3_t Rows[3];
    struct vector3_t;
};

void
do_stuff(struct mat3_t Matrix) {
    Matrix.x = 3;
}

vector3_t
vector3_add(vector3_t A, vector3_t B) {
    return (vector3_t){.x = A.x + B.x, .y = A.y + B.y, .z = A.z + B.z};
}

float
avg(int a, int b) {
    return (float)(a + b) / 2.f;
}


struct vector3_t 
make_vec(float x, float y, float z) {
    return (vector3_t){.x = x, .y = y, .z = z};
}

struct foo_t { int x; } funcy_stuff() { return (struct foo_t){123}; }

int main() {
    float (*func)(int, int) = avg;
    
    float f = func(32, 12);
    
    struct vector3_t v = {0};
    
    struct mat3_t Matrix = {0};
    vector3_t VecA = {.x = 19.f, .y = 23.f, .z = 0.321f};
    vector3_t VecB = {.x = .9f, .y = .20f, .z = 21.f};
    
    Matrix.Rows[0] = VecA;
    Matrix.Rows[1] = VecA;
    Matrix.Rows[2] = vector3_add(VecA, VecB);
    
    return 0;
}