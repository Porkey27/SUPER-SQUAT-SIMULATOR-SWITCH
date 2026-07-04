#include <glad/glad.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "gm_gl.h"

float GM_VIEW_ANGLE_SIGN   = 1.0f;
float GM_SPRITE_ANGLE_SIGN = 1.0f;

// ---- tiny column-major mat4 ------------------------------------------
typedef struct { float m[16]; } Mat4;

static Mat4 mat4_ortho(float l,float r,float b,float t)
{
    Mat4 o; memset(o.m,0,sizeof o.m);
    o.m[0]  = 2.0f/(r-l);
    o.m[5]  = 2.0f/(t-b);
    o.m[10] = -1.0f;
    o.m[12] = -(r+l)/(r-l);
    o.m[13] = -(t+b)/(t-b);
    o.m[15] = 1.0f;
    return o;
}
static Mat4 mat4_mul(const Mat4 a, const Mat4 b)   // a*b
{
    Mat4 o;
    for (int c=0;c<4;c++) for (int r=0;r<4;r++){
        float s=0; for(int k=0;k<4;k++) s += a.m[k*4+r]*b.m[c*4+k];
        o.m[c*4+r]=s;
    }
    return o;
}
static Mat4 mat4_translate(float x,float y)
{
    Mat4 o; memset(o.m,0,sizeof o.m);
    o.m[0]=o.m[5]=o.m[10]=o.m[15]=1.0f; o.m[12]=x; o.m[13]=y;
    return o;
}
static Mat4 mat4_rotz(float rad)
{
    Mat4 o; memset(o.m,0,sizeof o.m);
    float c=cosf(rad), s=sinf(rad);
    o.m[0]=c; o.m[1]=s; o.m[4]=-s; o.m[5]=c; o.m[10]=1; o.m[15]=1;
    return o;
}

// ---- GL objects -------------------------------------------------------
static GLuint prog, vao, vbo, uMVP, whiteTex;

#define MAXV 6144                 // 1024 quads
static float vbuf[MAXV * 8];      // x,y,u,v,r,g,b,a
static int   vcount;
static GLuint curTex; static int curBlend = -1;

static const char* VS =
 "#version 330 core\n"
 "layout(location=0) in vec2 aPos;\n"
 "layout(location=1) in vec2 aUV;\n"
 "layout(location=2) in vec4 aCol;\n"
 "uniform mat4 uMVP; out vec2 vUV; out vec4 vCol;\n"
 "void main(){ gl_Position=uMVP*vec4(aPos,0.0,1.0); vUV=aUV; vCol=aCol; }\n";
static const char* FS =
 "#version 330 core\n"
 "in vec2 vUV; in vec4 vCol; out vec4 frag; uniform sampler2D uTex;\n"
 "void main(){ frag=texture(uTex,vUV)*vCol; }\n";

static GLuint compile(GLenum t,const char* src)
{
    GLuint s=glCreateShader(t); glShaderSource(s,1,&src,NULL); glCompileShader(s);
    GLint ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){ char log[512]; glGetShaderInfoLog(s,512,NULL,log); printf("shader: %s\n",log); }
    return s;
}

void gmgl_init(void)
{
    GLuint vs=compile(GL_VERTEX_SHADER,VS), fs=compile(GL_FRAGMENT_SHADER,FS);
    prog=glCreateProgram(); glAttachShader(prog,vs); glAttachShader(prog,fs);
    glLinkProgram(prog); glDeleteShader(vs); glDeleteShader(fs);
    GLint linked; glGetProgramiv(prog,GL_LINK_STATUS,&linked);
    if(!linked){ char log[512]; glGetProgramInfoLog(prog,512,NULL,log); printf("PROGRAM LINK FAILED: %s\n",log); }
    else printf("program linked ok, prog=%u\n", prog);
    uMVP=glGetUniformLocation(prog,"uMVP");
    printf("uMVP location=%d\n", uMVP);

    glGenVertexArrays(1,&vao); glBindVertexArray(vao);
    glGenBuffers(1,&vbo); glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof vbuf,NULL,GL_STREAM_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(2*sizeof(float)));
    glVertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(4*sizeof(float)));
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glEnableVertexAttribArray(2);

    unsigned char wpix[4]={255,255,255,255};
    glGenTextures(1,&whiteTex); glBindTexture(GL_TEXTURE_2D,whiteTex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,wpix);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);

    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glEnable(GL_BLEND);
}

static void flush(void)
{
    if(!vcount) return;
    glUseProgram(prog); glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferSubData(GL_ARRAY_BUFFER,0,vcount*8*sizeof(float),vbuf);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,curTex);
    glDrawArrays(GL_TRIANGLES,0,vcount);
    vcount=0;
}
static void use_tex(GLuint t){ if(t!=curTex){ flush(); curTex=t; } }
static void use_blend(int b){
    if(b!=curBlend){ flush(); curBlend=b;
        // Separate alpha-channel blend so we never drag the window surface's
        // own destination alpha below 1 (the Switch compositor treats that as
        // window transparency, which made everything appear black).
        if(b==BM_ADD) glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE,               GL_ONE, GL_ZERO);
        else          glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
    }
}

void gmgl_set_projection(float angleDeg)
{
    flush();
    float w=view_wview, h=view_hview, cx=w*0.5f, cy=h*0.5f;
    Mat4 proj = mat4_ortho(0,w,h,0);
    Mat4 mv = mat4_mul(mat4_translate(cx,cy),
              mat4_mul(mat4_rotz(angleDeg*GM_VIEW_ANGLE_SIGN*GM_PI/180.0f),
              mat4_mul(mat4_translate(-cx,-cy),
                       mat4_translate(-view_xview,-view_yview))));
    Mat4 mvp = mat4_mul(proj,mv);
    glUseProgram(prog);
    glUniformMatrix4fv(uMVP,1,GL_FALSE,mvp.m);
}

static inline void vtx(float x,float y,float u,float v,GMColor c,float a)
{
    float* p=&vbuf[vcount*8];
    p[0]=x;p[1]=y;p[2]=u;p[3]=v;p[4]=c.r;p[5]=c.g;p[6]=c.b;p[7]=a;
    vcount++;
}
// emit a quad given 4 corners (0..3 = TL,TR,BR,BL) with uv per corner
static void quad(float px[4],float py[4],float u0,float u1,GMColor c,float a)
{
    if(vcount+6>MAXV) flush();
    float u[4]={u0,u1,u1,u0}, v[4]={0,0,1,1};
    vtx(px[0],py[0],u[0],v[0],c,a); vtx(px[1],py[1],u[1],v[1],c,a); vtx(px[2],py[2],u[2],v[2],c,a);
    vtx(px[0],py[0],u[0],v[0],c,a); vtx(px[2],py[2],u[2],v[2],c,a); vtx(px[3],py[3],u[3],v[3],c,a);
}

void gmgl_frame_begin(void)
{
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    curTex=0; curBlend=-1; use_blend(BM_NORMAL);
}
void gmgl_frame_end(void){ flush(); }

void gmgl_rectangle(float x1,float y1,float x2,float y2,GMColor col,float alpha,int blend)
{
    use_blend(blend); use_tex(whiteTex);
    float px[4]={x1,x2,x2,x1}, py[4]={y1,y1,y2,y2};
    quad(px,py,0,1,col,alpha);
}

void gmgl_sprite_stretched(const GMSprite* s,int sub,float x,float y,float w,float h,
                           GMColor col,float alpha,int blend)
{
    use_blend(blend); use_tex(s->tex);
    float u0,u1; gmsprite_uv(s,sub,&u0,&u1);
    float px[4]={x,x+w,x+w,x}, py[4]={y,y,y+h,y+h};
    quad(px,py,u0,u1,col,alpha);
}

static inline void rot(float dx,float dy,float deg,float* ox,float* oy)
{
    float r=deg*GM_SPRITE_ANGLE_SIGN*GM_PI/180.0f, c=cosf(r), s=sinf(r);
    *ox = dx*c + dy*s;
    *oy = -dx*s + dy*c;
}

void gmgl_sprite_ext(const GMSprite* s,int sub,float x,float y,float xs,float ys,
                     float angDeg,GMColor col,float alpha,int blend)
{
    use_blend(blend); use_tex(s->tex);
    float u0,u1; gmsprite_uv(s,sub,&u0,&u1);
    float fw=s->frameW, fh=s->frameH, ox=s->originX, oy=s->originY;
    float cx[4]={0,fw,fw,0}, cy[4]={0,0,fh,fh}, px[4], py[4];
    for(int i=0;i<4;i++){
        float dx=(cx[i]-ox)*xs, dy=(cy[i]-oy)*ys, rx,ry;
        rot(dx,dy,angDeg,&rx,&ry);
        px[i]=x+rx; py[i]=y+ry;
    }
    quad(px,py,u0,u1,col,alpha);
}

void gmgl_sprite_default(const GMSprite* s,float image_index,float x,float y,
                         float xs,float ys,float angDeg,GMColor blend,float alpha)
{
    gmgl_sprite_ext(s,(int)image_index,x,y,xs,ys,angDeg,blend,alpha,BM_NORMAL);
}

// sprite-font text: '#' newline; frame = ord(c)-33 ("!" first). fixed cell.
void gmgl_text_transformed(const GMSprite* f,float x,float y,const char* str,
                           float xs,float ys,float angDeg)
{
    if(!f || !str) return;
    float cw=f->frameW+GM_FONT_SEP, lh=f->frameH+GM_FONT_SEP;
    // count lines and per-line lengths
    int nlines=1; for(const char* p=str;*p;p++) if(*p=='#') nlines++;
    float totalH=nlines*lh;
    float vOff = draw_valign==FA_MIDDLE ? -totalH*0.5f : (draw_valign==FA_BOTTOM ? -totalH : 0);

    int li=0; const char* p=str;
    while(1){
        // measure this line
        const char* q=p; int len=0; while(q[len] && q[len]!='#') len++;
        float lineW=len*cw;
        float hOff = draw_halign==FA_CENTER ? -lineW*0.5f : (draw_halign==FA_RIGHT ? -lineW : 0);
        for(int ci=0;ci<len;ci++){
            char ch=p[ci]; int frame=(int)ch-33;
            if(frame<0 || frame>=f->frames){ continue; }   // space / out of range
            float lx=hOff+ci*cw, ly=vOff+li*lh;
            // glyph quad: corners in text space, scaled, rotated about (x,y)
            float u0,u1; gmsprite_uv(f,frame,&u0,&u1);
            use_tex(f->tex); use_blend(BM_NORMAL);
            float cx[4]={0,f->frameW,f->frameW,0}, cy[4]={0,0,f->frameH,f->frameH}, px[4],py[4];
            for(int k=0;k<4;k++){
                float gx=(lx+cx[k])*xs, gy=(ly+cy[k])*ys, rx,ry;
                rot(gx,gy,angDeg,&rx,&ry);
                px[k]=x+rx; py[k]=y+ry;
            }
            quad(px,py,u0,u1,draw_color,draw_alpha);
        }
        p += len;
        if(*p!='#') break;
        p++; li++;
    }
}
