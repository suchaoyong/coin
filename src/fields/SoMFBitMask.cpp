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

/*!
  \class SoMFBitMask SoMFBitMask.h Inventor/fields/SoMFBitMask.h
  \brief The SoMFBitMask class ...
  \ingroup fields

  FIXME: write class doc
*/

#include <Inventor/fields/SoMFBitMask.h>
#include <Inventor/fields/SoSFBitMask.h>

#include <Inventor/errors/SoDebugError.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoOutput.h>
#include <Inventor/SbName.h>



SO_MFIELD_DERIVED_SOURCE(SoMFBitMask, not_used, not_used);


/*!
  Does initialization common for all objects of the
  SoMFBitMask class. This includes setting up the
  type system, among other things.
*/
void
SoMFBitMask::initClass(void)
{
  SO_MFIELD_INIT_CLASS(SoMFBitMask, inherited);
}

SbBool
SoMFBitMask::read1Value(SoInput * in, int idx)
{
  SoSFBitMask sfbitmask;
  sfbitmask.setEnums(this->numEnums, this->enumValues, this->enumNames);
  SbBool result;
  if ((result = sfbitmask.readValue(in)))
    this->set1Value(idx, sfbitmask.getValue());
  return result;
}

void
SoMFBitMask::write1Value(SoOutput * out, int idx) const
{
  SoSFBitMask sfbitmask;
  sfbitmask.setEnums(this->numEnums, this->enumValues, this->enumNames);
  sfbitmask.setValue((*this)[idx]);
  sfbitmask.writeValue(out);
}

/*!
  FIXME: write function documentation
*/
SbBool
SoMFBitMask::findEnumValue(const SbName & name, int & val)
{
  // Look through names table for one that matches
  for (int i = 0; i < numEnums; i++) {
    if (name == enumNames[i]) {
      val = enumValues[i];
      return TRUE;
    }
  }
  return FALSE;
}

void
SoMFBitMask::convertTo(SoField * dest) const
{
  if (dest->getTypeId()==SoSFBitMask::getClassTypeId()) {
    if (this->getNum()>0)
      ((SoSFBitMask *)dest)->setValue((*this)[0]);
  }
#if COIN_DEBUG
  else {
    SoDebugError::post("SoMFBitMask::convertTo",
                       "Can't convert from %s to %s",
                       this->getTypeId().getName().getString(),
                       dest->getTypeId().getName().getString());
  }
#endif // COIN_DEBUG
}
