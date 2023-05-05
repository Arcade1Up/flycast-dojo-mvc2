/*
    Created on: Oct 18, 2019

	Copyright 2019 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef LIBRETRO
#include "context.h"
#include "cfg/option.h"

#include "gl_context.h"

GraphicsContext *GraphicsContext::instance;

void initRenderApi(void *window, void *display) {
#ifdef USE_OPENGL
  if (!isOpenGL(config::RendererType))
    config::RendererType = RenderType::OpenGL;
  theGLContext.setWindow(window, display);
  if (theGLContext.init())
    return;
#endif
  die("Cannot initialize the graphics API");
}

void termRenderApi() {
  if (GraphicsContext::Instance() != nullptr)
    GraphicsContext::Instance()->term();
  verify(GraphicsContext::Instance() == nullptr);
}

void switchRenderApi() {
  void *window = nullptr;
  void *display = nullptr;
  if (GraphicsContext::Instance() != nullptr)
    GraphicsContext::Instance()->getWindow(&window, &display);
  termRenderApi();
  initRenderApi(window, display);
}
#endif
