/**************************************************************************\
 *
 *  Copyright (C) 1998-1999 by Systems in Motion.  All rights reserved.
 *
 *  This file is part of the Coin library.
 *
 *  This file may be distributed under the terms of the Q Public License
 *  as defined by Troll Tech AS of Norway and appearing in the file
 *  LICENSE.QPL included in the packaging of this file.
 *
 *  If you want to use Coin in applications not covered by licenses
 *  compatible with the QPL, you can contact SIM to aquire a
 *  Professional Edition license for Coin.
 *
 *  Systems in Motion AS, Prof. Brochs gate 6, N-7030 Trondheim, NORWAY
 *  http://www.sim.no/ sales@sim.no Voice: +47 22114160 Fax: +47 67172912
 *
\**************************************************************************/

#ifndef __SOSELECTONE_H__
#define __SOSELECTONE_H__

#include <Inventor/engines/SoSubEngine.h>
#include <Inventor/fields/SoMField.h>
#include <Inventor/fields/SoSFInt32.h>

#if defined(COIN_EXCLUDE_SOSELECTONE)
#error "Configuration settings disrespected -- do not include this file!"
#endif // COIN_EXCLUDE_SOSELECTONE

class SoEngineOutput;

class SoSelectOne : public SoEngine {
  typedef SoEngine inherited;
  SO_ENGINE_ABSTRACT_HEADER(SoSelectOne);

public:
  SoSFInt32 index;
  SoMField * input;

  SoEngineOutput * output;

  SoSelectOne(SoType inputType);

  static void initClass();

private:
  SoSelectOne(void);
  ~SoSelectOne();
  virtual void evaluate();

  // Avoid a g++/egcs warning.
  friend class dummy;
};

#endif // !__SOSELECTONE_H__
