#define IN_CUSES_MODULE
#include "CUSESImpl.hpp"
#include "CUSESBootstrap.hpp"
#include "IfaceAnnoTools.hxx"
#include "AnnoToolsBootstrap.hpp"
#include <algorithm>
#include <math.h>

CDAUserBaseUnit::CDAUserBaseUnit(iface::cellml_api::Units* aBaseUnits)
  throw()
  : _cda_refcount(1), mBaseUnits(aBaseUnits)
{
}

CDAUserBaseUnit::~CDAUserBaseUnit()
  throw()
{
}

wchar_t*
CDAUserBaseUnit::name()
  throw(std::exception&)
{
  return CDA_wcsdup(mBaseUnits->name());
}

iface::cellml_api::Units*
CDAUserBaseUnit::cellmlUnits()
  throw(std::exception&)
{
  mBaseUnits->add_ref();
  return mBaseUnits;
}

// Next, the built in base units. These get defined by macro...
#define BASE_UNIT(x) \
  class CDABuiltinBaseUnit##x \
    : public iface::cellml_services::BaseUnit \
  { \
  public:\
    void add_ref() throw() {} \
    void release_ref() throw() {} \
    CDA_IMPL_ID \
    CDA_IMPL_QI1(cellml_services::BaseUnit) \
    CDABuiltinBaseUnit##x() throw() {} \
    ~CDABuiltinBaseUnit##x() throw() {} \
    \
    wchar_t* name() throw() { return CDA_wcsdup(L###x); } \
  }; \
  CDABuiltinBaseUnit##x gBuiltinBase##x;

BASE_UNIT(ampere);
BASE_UNIT(candela);
BASE_UNIT(kelvin);
BASE_UNIT(kilogram);
BASE_UNIT(metre);
BASE_UNIT(mole);
BASE_UNIT(second);

CDABaseUnitInstance::CDABaseUnitInstance
(
 iface::cellml_services::BaseUnit* aBaseUnit,
 double aPrefix,
 double aOffset,
 double aExponent
)
  throw()
  : _cda_refcount(1), mBaseUnit(aBaseUnit), mPrefix(aPrefix), mOffset(aOffset),
    mExponent(aExponent)
{
}

CDABaseUnitInstance::~CDABaseUnitInstance()
  throw()
{
}

iface::cellml_services::BaseUnit*
CDABaseUnitInstance::unit()
  throw(std::exception&)
{
  mBaseUnit->add_ref();
  return mBaseUnit;
}

double
CDABaseUnitInstance::prefix()
  throw(std::exception&)
{
  return mPrefix;
}

double
CDABaseUnitInstance::offset()
  throw(std::exception&)
{
  return mOffset;
}

double
CDABaseUnitInstance::exponent()
  throw(std::exception&)
{
  return mExponent;
}

CDACanonicalUnitRepresentation::CDACanonicalUnitRepresentation(bool aStrict)
  throw()
  : _cda_refcount(1), mStrict(aStrict)
{
}

CDACanonicalUnitRepresentation::~CDACanonicalUnitRepresentation()
  throw()
{
  std::vector<iface::cellml_services::BaseUnitInstance*>::iterator i;
  for (i = baseUnits.begin(); i != baseUnits.end(); i++)
    (*i)->release_ref();
}

uint32_t
CDACanonicalUnitRepresentation::length()
  throw(std::exception&)
{
  return baseUnits.size();
}

iface::cellml_services::BaseUnitInstance*
CDACanonicalUnitRepresentation::fetchBaseUnit(uint32_t aIndex)
  throw(std::exception&)
{
  if (aIndex >= baseUnits.size())
    throw iface::cellml_api::CellMLException();
  iface::cellml_services::BaseUnitInstance* bui = baseUnits[aIndex];
  bui->add_ref();
  return bui;
}

bool
CDACanonicalUnitRepresentation::compatibleWith
(
 iface::cellml_services::CanonicalUnitRepresentation* aCompareWith
)
  throw(std::exception&)
{
  uint32_t l = length();
  if (l != aCompareWith->length())
    return false;

  uint32_t i;
  for (i = 0; i < l; i++)
  {
    RETURN_INTO_OBJREF(bui1, iface::cellml_services::BaseUnitInstance,
                       aCompareWith->fetchBaseUnit(i));

    iface::cellml_services::BaseUnitInstance* bui2 = baseUnits[i];

    RETURN_INTO_OBJREF(u1, iface::cellml_services::BaseUnit, bui1->unit());
    RETURN_INTO_OBJREF(u2, iface::cellml_services::BaseUnit, bui2->unit());

    if (CDA_objcmp(u1, u2) != 0)
      return false;

    if (bui1->exponent() != bui2->exponent())
      return false;

    if (mStrict &&
        (bui1->offset() != bui2->offset() ||
         bui1->prefix() != bui2->prefix()))
      return false;
  }

  return true;
}

double
CDACanonicalUnitRepresentation::convertUnits
(
 iface::cellml_services::CanonicalUnitRepresentation* aCompareWith,
 double* aOffset
)
  throw(std::exception&)
{
  double m1, m2, o1, o2;
  m1 = siConversion(&o1);
  m2 = aCompareWith->siConversion(&o2);

  double fac = m1/m2;

  *aOffset = o1 - fac * o2;

  return fac;
}

double
CDACanonicalUnitRepresentation::siConversion
(
 double* aOffset
)
  throw(std::exception&)
{
  uint32_t l = length();
  if (l == 1 && baseUnits[0]->exponent() == 1.0)
  {
    double o, m;
    o = baseUnits[0]->offset();
    m = baseUnits[0]->prefix();
    *aOffset = -o / m;
    return 1.0 / m;
  }

  *aOffset = 0.0;

  double ret = 1.0;

  uint32_t i;
  for (i = 0; i < l; i++)
  {
    iface::cellml_services::BaseUnitInstance* bui = baseUnits[i];
    ret *= bui->prefix();
  }

  return 1.0 / ret;
}

struct CanonicalUnitComparator
{
public:
  bool
  operator ()(iface::cellml_services::BaseUnitInstance* x,
              iface::cellml_services::BaseUnitInstance* y)
  {
    RETURN_INTO_OBJREF(xu, iface::cellml_services::BaseUnit,
                       x->unit());
    RETURN_INTO_OBJREF(yu, iface::cellml_services::BaseUnit,
                       y->unit());

    int ret = CDA_objcmp(xu, yu);
    if (ret != 0)
      return (ret < 0);

    // We have two identical units, but in strict mode this is a perfectly
    // valid final state. Sort by prefix...
    return (x->prefix() < y->prefix());
  }
};

void
CDACanonicalUnitRepresentation::canonicalise()
  throw(std::exception&)
{
  CanonicalUnitComparator cuc;
  std::sort(baseUnits.begin(), baseUnits.end(), cuc);

  bool anyChanges = false;
  std::vector<iface::cellml_services::BaseUnitInstance*> newBaseUnits;

  uint32_t l = length();
  uint32_t i;

  iface::cellml_services::BaseUnitInstance *uThis, *uLast = NULL;

  for (i = 0; i < l; i++)
  {
    uThis = baseUnits[i];
    if (uLast != NULL)
    {
      if (!mStrict || uLast->prefix() != uThis->prefix())
      {
        RETURN_INTO_OBJREF(buLast, iface::cellml_services::BaseUnit,
                           uLast->unit());
        RETURN_INTO_OBJREF(buThis, iface::cellml_services::BaseUnit,
                           uThis->unit());
        if (CDA_objcmp(buLast, buThis) == 0)
        {
          anyChanges = true;
          newBaseUnits.pop_back();
          double newPref = uThis->prefix() * uLast->prefix();
          double newExp = uThis->exponent() + uLast->exponent();
          uLast->release_ref();
          uThis->release_ref();
          uLast = new CDABaseUnitInstance(buThis, newPref, 0.0, newExp);
          newBaseUnits.push_back(uLast);
          continue;
        }
      }
    }
    newBaseUnits.push_back(uThis);
    uLast = uThis;
  }

  if (anyChanges)
    baseUnits = newBaseUnits;
}

void
CDACanonicalUnitRepresentation::addBaseUnit
(
 iface::cellml_services::BaseUnitInstance* baseUnit
)
{
  baseUnit->add_ref();
  baseUnits.push_back(baseUnit);
}

CDACUSES::CDACUSES(iface::cellml_api::Model* aModel, bool aStrict)
  throw()
  : _cda_refcount(1), mStrict(aStrict)
{
  PopulateBuiltinUnits();

  RETURN_INTO_OBJREF(ats, iface::cellml_services::AnnotationToolService,
                     CreateAnnotationToolService());

  RETURN_INTO_OBJREF(cusesAS, iface::cellml_services::AnnotationSet,
                     ats->createAnnotationSet());

  ScopeMap<iface::cellml_api::Units> unitsMap;

  aModel->fullyInstantiateImports();

  RETURN_INTO_OBJREF(us, iface::cellml_api::UnitsSet,
                     aModel->allUnits());
  RETURN_INTO_OBJREF(ui, iface::cellml_api::UnitsIterator,
                     us->iterateUnits());
  while (true)
  {
    RETURN_INTO_OBJREF(u, iface::cellml_api::Units, ui->nextUnits());
    if (u == NULL)
      break;
    
    u->add_ref();
    unitsMap->insert(std::pair<std::wstring, iface::cellml_api::Units*>
                    (getUnitScope(u), u.getPointer()));

    RETURN_INTO_WSTRING(currentName, u->name());
    ObjRef<iface::cellml_api::CellMLElement> el(u);
    while (true)
    {
      RETURN_INTO_OBJREF(par, iface::cellml_api::CellMLElement,
                         el->parentElement());
      if (par == NULL)
        break;
      el = par;

      DECLARE_QUERY_INTERFACE_OBJREF(imp, el, cellml_api::CellMLImport);
      if (imp == NULL)
        continue;

      RETURN_INTO_OBJREF(ius, iface::cellml_api::ImportUnitsSet,
                         imp->units());
      RETURN_INTO_OBJREF(iui, iface::cellml_api::ImportUnitsIterator,
                         ius->iterateImportUnits());
      bool hit = false;
      while (true)
      {
        RETURN_INTO_OBJREF(iu, iface::cellml_api::ImportUnits,
                           iui->nextImportUnits());
        if (iu == NULL)
          break;

        RETURN_INTO_WSTRING(unitsRef, iu->unitsRef());
        if (unitsRef == currentName)
        {
          wchar_t* str = iu->name();
          currentName = str;
          free(str);

          RETURN_INTO_OBJREF(mod, iface::cellml_api::CellMLElement,
                             imp->parentElement());
          
          std::wstring scopedName(getUnitScope(mod));
          if (scopedName != L"")
            scopedName += L'/';
          scopedName += currentName;

          u->add_ref();
          unitsMap->insert(std::pair<std::wstring, iface::cellml_api::Units*>
                           (scopedName, u.getPointer()));

          hit = true;
          break;
        }
      }

      if (!hit)
        break;
    }
  }

  RETURN_INTO_OBJREF(sentinel, UnitDependencies, new UnitDependencies());

  std::multimap<std::wstring, iface::cellml_api::Units*>::const_iterator umi;
  for (umi = unitsMap->begin(); umi != unitsMap->end(); umi++)
  {
    RETURN_INTO_OBJREF(depsX, iface::XPCOM::IObject,
                       cusesAS->getObjectAnnotation((*umi).second, L"dependencies"));
    // Only need to annotate each unit once...
    if (depsX != NULL)
    {
      dynamic_cast<UnitDependencies*>(depsX.getPointer())->scopes.
        push_back((*umi).first);
      continue;
    }

    sentinel->addDependency((*umi).second);

    RETURN_INTO_OBJREF(deps, UnitDependencies, new UnitDependencies());
    cusesAS->setObjectAnnotation((*umi).second, L"dependencies", deps);

    wchar_t* str = (*umi).second->name();
    deps->name = str;
    free(str);

    deps->scopes.push_back((*umi).first);

    RETURN_INTO_OBJREF(us, iface::cellml_api::UnitSet,
                       (*umi).second->unitCollection());
    RETURN_INTO_OBJREF(ui, iface::cellml_api::UnitIterator, us->iterateUnits());
    while (true)
    {
      RETURN_INTO_OBJREF(un, iface::cellml_api::Unit, ui->nextUnit());
      if (un == NULL)
        break;

      RETURN_INTO_WSTRING(uname, un->units());

      if (uname == L"")
      {
        warningDescription += L"Found a unit with no units attribute in units ";
        warningDescription += deps->name;
        warningDescription += L"; ";
        continue;
      }

      iface::cellml_api::Units* ufound =
        scopedFind(unitsMap, (*umi).second, uname);
      if (ufound == NULL && BuiltinUnit(uname))
        continue;

      if (ufound == NULL)
      {
        errorDescription += L"Units ";
        RETURN_INTO_WSTRING(refu, (*umi).second->name());
        errorDescription += refu;
        errorDescription += L" references units ";
        errorDescription += uname;
        errorDescription += L" but the latter units could not be found";
        errorDescription += L"; ";
        continue;
      }

      deps->addDependency(ufound);
    }
  }

  if (errorDescription != L"")
  {
    return;
  }

  // DFS visit all units...
  dfsResolveUnits(cusesAS, sentinel);
}

CDACUSES::~CDACUSES()
  throw()
{
}

template<class C>
C*
CDACUSES::scopedFind
(
 ScopeMap<C>& aMap,
 iface::cellml_api::CellMLElement* aContext,
 std::wstring& aName 
)
{
  std::wstring contextScope = getUnitScope(aContext);
  while (true)
  {
    std::wstring full = contextScope;
    if (contextScope != L"")
      full += L'/';
    full += aName;

    typename std::multimap<std::wstring, C*>::iterator i = aMap->find(full);
    if (i != aMap->end())
    {
      return (*i).second;
    }

    size_t idx = contextScope.rfind(L'/');
    if (idx == std::wstring::npos)
    {
      if (contextScope == L"")
        break;
      contextScope = L"";
    }
    else
      contextScope = contextScope.substr(0, idx);
  }

  return NULL;
}

std::wstring
CDACUSES::getUnitScope(iface::cellml_api::CellMLElement* aContext)
{
  std::wstring scope;
  ObjRef<iface::cellml_api::CellMLElement> cur(aContext);

  DECLARE_QUERY_INTERFACE_OBJREF(u, aContext, cellml_api::Units);
  if (u != NULL)
  {
    wchar_t* str = u->name();
    scope = str;
    free(str);
  }

  do
  {
    DECLARE_QUERY_INTERFACE_OBJREF(comp, cur, cellml_api::CellMLComponent);
    if (comp)
    {
      RETURN_INTO_WSTRING(sc, comp->name());
      std::wstring newScope = L"comp_";
      newScope += sc;
      newScope += L'/';
      scope = newScope + scope;
    }
    else
    {
      DECLARE_QUERY_INTERFACE_OBJREF(imp, cur, cellml_api::CellMLImport);

      if (imp != NULL)
      {
        // We name the import by the first import component, or if there is no
        // such component, by the first import unit.
        RETURN_INTO_OBJREF(ics, iface::cellml_api::ImportComponentSet,
                           imp->components());
        RETURN_INTO_OBJREF(ici, iface::cellml_api::ImportComponentIterator,
                           ics->iterateImportComponents());
        RETURN_INTO_OBJREF(ic, iface::cellml_api::ImportComponent,
                           ici->nextImportComponent());
        if (ic == NULL)
        {
          RETURN_INTO_OBJREF(ius, iface::cellml_api::ImportUnitsSet,
                             imp->units());
          RETURN_INTO_OBJREF(iui, iface::cellml_api::ImportUnitsIterator,
                             ius->iterateImportUnits());
          RETURN_INTO_OBJREF(iu, iface::cellml_api::ImportUnits,
                             iui->nextImportUnits());
          if (iu != NULL)
          {
            RETURN_INTO_WSTRING(sc, iu->name());
            std::wstring newScope = L"imp_byunits_";
            newScope += sc;
            newScope += L'/';
            scope = newScope + scope;
          }
        }
        else
        {
          RETURN_INTO_WSTRING(sc, ic->name());
          std::wstring newScope = L"imp_bycomp_";
          newScope += sc;
          newScope += L'/';
          scope = newScope + scope;
        }
      }
    }

    RETURN_INTO_OBJREF(par, iface::cellml_api::CellMLElement,
                       cur->parentElement());
    cur = par;
  }
  while (cur != NULL);

  return scope;
}

bool
CDACUSES::BuiltinUnit(std::wstring& aName)
{
  // ampere  	   farad  	katal  	 lux  	pascal    tesla
  // becquerel 	   gram	        kelvin 	 meter 	radian 	  volt
  // candela 	   gray	        kilogram metre 	second 	  watt
  // celsius 	   henry 	liter 	 mole 	siemens   weber
  // coulomb 	   hertz 	litre 	 newton sievert 	 
  // dimensionless joule 	lumen 	 ohm 	steradian 	 
  if (aName[0] < L'l')
  {
    if (aName[0] < L'g')
    {
      if (aName[0] == 'c')
      {
        if (aName[1] == 'a')
          return aName == L"candela";
        else if (aName[1] == 'e')
          return aName == L"celsius";
        else
          return aName == L"coulomb";
      }
      else if (aName[0] <= 'b')
      {
        if (aName[0] == 'a')
          return aName == L"ampere";
        else
          return aName == L"becquerel";
      }
      else if (aName[0] == 'd')
      {
        return aName == L"dimensionless";
      }
      else
        return aName == L"farad";
    }
    else if (aName[0] < L'k')
    {
      if (aName[0] == L'g')
        return (aName.size() == 4 &&
                aName[1] == L'r' && aName[2] == L'a' &&
                (aName[3] == L'm' || aName[3] == L'y'));
      else if (aName[0] == L'j')
        return aName == L"joule";
      else if (aName[1] == L'e')
      {
        if (aName[2] == 'n')
          return aName == L"henry";
        else
          return aName == L"hertz";
      }
      else
        return false;
    }
    else if (aName[1] < L'i')
    {
      if (aName[1] == L'a')
        return aName == L"katal";
      else
        return aName == L"kelvin";
    }
    else
      return aName == L"kilogram";
  }
  else if (aName[0] < L'p')
  {
    if (aName[0] == L'l')
    {
      if (aName[1] == L'i')
      {
        if (aName[2] != L't' ||
            aName.size() != 5)
          return false;
        return (aName[3] == L'e' && aName[4] == L'r') ||
          (aName[3] == L'r' && aName[4] == L'e');
      }
      else if (aName[1] == L'u')
      {
        if (aName[2] == 'm')
          return aName == L"lumen";
        else
          return aName == L"lux";
      }
      else
        return false;
    }
    else if (aName[0] == L'm')
    {
      if (aName[1] == L'e')
      {
        if (aName[2] != L't' || aName.size() != 5)
          return false;
        return (aName[3] == L'r' && aName[4] == L'e') ||
          (aName[3] == L'e' && aName[4] == L'r');
      }
      else
        return aName == L"mole";
    }
    else if (aName[0] == L'n')
      return aName == L"newton";
    else
      return aName == L"ohm";
  }
  else if (aName[0] < L't')
  {
    if (aName[0] == L's')
    {
      if (aName[1] == L'i')
      {
        if (aName[2] != L'e')
          return false;
        if (aName[3] == L'm')
          return aName == L"siemens";
        else
          return aName == L"sievert";
      }
      else if (aName[1] == L'e')
        return aName == L"second";
      else
        return aName == L"steradian";
    }
    else if (aName[0] == L'p')
      return aName == L"pascal";
    else
      return aName == L"radian";
  }
  else if (aName[0] < L'w')
  {
    if (aName[0] == L't')
      return aName == L"tesla";
    else
      return aName == L"volt";
  }
  else if (aName[1] == L'a')
    return aName == L"watt";
  else
    return aName == L"weber";
}

bool
CDACUSES::dfsResolveUnits(iface::cellml_services::AnnotationSet* cusesAS,
                          UnitDependencies* search)
{
  if (search->done)
    return true;

  if (search->seen)
  {
    errorDescription += L"Units are defined circularly. One unit in the "
                        L"cycle is ";
    errorDescription += search->name;
    errorDescription += L"; ";
    return false;
  }

  std::list<iface::cellml_api::Units*>::iterator i;
  search->seen = true;

  for (i = search->dependencies.begin(); i != search->dependencies.end(); i++)
  {
    // Instantiate first, then add the unit...
    iface::cellml_api::Units* u = *i;

    RETURN_INTO_OBJREF(depsX, iface::XPCOM::IObject,
                       cusesAS->getObjectAnnotation(*i, L"dependencies"));
    UnitDependencies* deps = dynamic_cast<UnitDependencies*>(depsX.getPointer());

    if (!dfsResolveUnits(cusesAS, deps))
      return false;

    ComputeUnits(deps, u);
  }

  search->done = true;

  return true;
}

void
CDACUSES::PopulateBuiltinUnits()
{
#define BUILTIN_UNIT(name, x) \
  { \
    RETURN_INTO_OBJREF(cu, CDACanonicalUnitRepresentation, \
                       new CDACanonicalUnitRepresentation(mStrict)); \
    x \
    cu->canonicalise(); \
    mUnitsMap->insert(std::pair<std::wstring, CDACanonicalUnitRepresentation*> \
                      (L###name, cu)); \
    cu->add_ref(); \
  }
#define DERIVES(name, factor, exponent, offset) \
  { \
    RETURN_INTO_OBJREF(bui, CDABaseUnitInstance, \
      new CDABaseUnitInstance(&gBuiltinBase##name, factor, offset, exponent)); \
    cu->addBaseUnit(bui); \
  }

BUILTIN_UNIT(ampere, DERIVES(ampere, 1, 1, 0));
BUILTIN_UNIT(becquerel, DERIVES(second, 1, -1, 0));
BUILTIN_UNIT(candela, DERIVES(candela, 1, 1, 0));
BUILTIN_UNIT(celsius, DERIVES(kelvin, 1, 1, -273.15));
BUILTIN_UNIT(coulomb, DERIVES(ampere, 1, 1, 0) DERIVES(second, 1, 1, 0));
BUILTIN_UNIT(dimensionless, ;);
BUILTIN_UNIT(farad , DERIVES(metre, 1, -2, 0) DERIVES(kilogram, 1, -1, 0) DERIVES(second, 1, 4, 0) DERIVES(ampere, 1, 2, 0));
BUILTIN_UNIT(gram, DERIVES(kilogram, 1000, 1, 0));
BUILTIN_UNIT(gray, DERIVES(metre, 1, 2, 0) DERIVES(second, 1, -2, 0));
BUILTIN_UNIT(henry, DERIVES(metre, 1, 2, 0) DERIVES(kilogram, 1, 1, 0) DERIVES(second, 1, -2, 0) DERIVES(ampere, 1, -2, 0));
BUILTIN_UNIT(hertz, DERIVES(second, 1, -1, 0));
BUILTIN_UNIT(joule, DERIVES(metre, 1, 2, 0) DERIVES(kilogram, 1, 1, 0) DERIVES(second, 1, -2, 0));
BUILTIN_UNIT(katal, DERIVES(second, 1, -1, 0) DERIVES(mole, 1, 1, 0));
BUILTIN_UNIT(kelvin, DERIVES(kelvin, 1, 1, 0));
BUILTIN_UNIT(kilogram, DERIVES(kilogram, 1, 1, 0));
BUILTIN_UNIT(liter, DERIVES(metre, 1000, 3, 0));
BUILTIN_UNIT(litre, DERIVES(metre, 1000, 3, 0));
BUILTIN_UNIT(lumen, DERIVES(candela, 1, 1, 0));
BUILTIN_UNIT(lux, DERIVES(candela, 1, 1, 0) DERIVES(metre, 1, -2, 0));
BUILTIN_UNIT(meter, DERIVES(metre, 1, 1, 0));
BUILTIN_UNIT(metre, DERIVES(metre, 1, 1, 0));
BUILTIN_UNIT(mole, DERIVES(mole, 1, 1, 0));
BUILTIN_UNIT(newton, DERIVES(metre, 1, 1, 0) DERIVES(kilogram, 1, 1, 0) DERIVES(second, 1, -2, 0));
BUILTIN_UNIT(ohm, DERIVES(metre, 1, 2, 0) DERIVES(kilogram, 1, 1, 0) DERIVES(second, 1, -3, 0) DERIVES(ampere, 1, -2, 0));
BUILTIN_UNIT(pascal, DERIVES(metre, 1, -1, 0) DERIVES(kilogram, 1, 1, 0) DERIVES(second, 1, -2, 0));
BUILTIN_UNIT(radian, ;);
BUILTIN_UNIT(second, DERIVES(second, 1, 1, 0));
BUILTIN_UNIT(siemens, DERIVES(metre, 1, -2, 0) DERIVES(kilogram, 1, -1, 0) DERIVES(second, 1, 3, 0) DERIVES(ampere, 1, 2, 0));
BUILTIN_UNIT(sievert, DERIVES(metre, 1, 2, 0) DERIVES(second, 1, -2, 0));
BUILTIN_UNIT(steradian, ;);
BUILTIN_UNIT(tesla, DERIVES(kilogram, 1, 1, 0) DERIVES(second, 1, -2, 0) DERIVES(ampere, 1, -1, 0));
BUILTIN_UNIT(volt, DERIVES(metre, 1, 2, 0) DERIVES(kilogram, 1, 1, 0) DERIVES(second, 1, -3, 0) DERIVES(ampere, 1, -1, 0));
BUILTIN_UNIT(watt, DERIVES(metre, 1, 2, 0) DERIVES(kilogram, 1, 1, 0) DERIVES(second, 1, -3, 0));
BUILTIN_UNIT(weber, DERIVES(metre, 1, 2, 0) DERIVES(kilogram, 1, 1, 0) DERIVES(second, 1, -2, 0) DERIVES(ampere, 1, -1, 0));
#undef DERIVES
#undef BUILTIN_UNIT
}

void
CDACUSES::ComputeUnits
(
 UnitDependencies* context,
 iface::cellml_api::Units* units
)
{
  RETURN_INTO_OBJREF(newrep, CDACanonicalUnitRepresentation,
                     new CDACanonicalUnitRepresentation(mStrict));

  // All units defined in units should now exist (it is an internal error if
  // not), so use this to define the current unit.

  RETURN_INTO_OBJREF(us, iface::cellml_api::UnitSet,
                     units->unitCollection());
  RETURN_INTO_OBJREF(ui, iface::cellml_api::UnitIterator,
                     us->iterateUnits());
  while (true)
  {
    RETURN_INTO_OBJREF(u, iface::cellml_api::Unit,
                       ui->nextUnit());
    if (u == NULL)
      break;

    // Find the unit...
    RETURN_INTO_WSTRING(uname, u->units());

    if (uname == L"")
      continue;

    RETURN_INTO_OBJREF(urep, iface::cellml_services::CanonicalUnitRepresentation,
                       getUnitsByName(units, uname.c_str()));
    // If urep is null, then something is wrong internally, because we have
    // already checked the names are valid and resolved all dependency names.
    uint32_t l = urep->length();
    uint32_t i;
    for (i = 0; i < l; i++)
    {
      RETURN_INTO_OBJREF(bu, iface::cellml_services::BaseUnitInstance,
                         urep->fetchBaseUnit(i));
      double newExponent = u->exponent() * bu->exponent();
      double newPrefix;
      if (i == 0)
        newPrefix = u->multiplier() *
          pow(pow(10.0, -u->prefix()) * bu->prefix(), u->exponent());
      else
        newPrefix = pow(bu->prefix(), u->exponent());
      double newOffset;
      if (i == 0)
        newOffset = u->offset() + bu->offset();
      else
        newOffset = bu->offset();

      RETURN_INTO_OBJREF(ub, iface::cellml_services::BaseUnit,
                         bu->unit());
      RETURN_INTO_OBJREF(nbu, iface::cellml_services::BaseUnitInstance,
                         new CDABaseUnitInstance(ub, newPrefix, newOffset,
                                                 newExponent));
      newrep->addBaseUnit(nbu);
    }
  }

  newrep->canonicalise();

  std::list<std::wstring>::iterator i(context->scopes.begin());
  for (; i != context->scopes.end(); i++)
  {
    mUnitsMap->insert(std::pair<std::wstring, CDACanonicalUnitRepresentation*>
                      (*i, newrep.getPointer()));
    newrep->add_ref();
  }
}

iface::cellml_services::CanonicalUnitRepresentation*
CDACUSES::getUnitsByName
(
 iface::cellml_api::CellMLElement* aContext,
 const wchar_t* aName
)
  throw(std::exception&)
{
  std::wstring name(aName);
  iface::cellml_services::CanonicalUnitRepresentation* cur =
    scopedFind(mUnitsMap, aContext, name);
  if (cur != NULL)
    cur->add_ref();
  
  return cur;
}

wchar_t*
CDACUSES::modelError()
  throw(std::exception&)
{
  std::wstring tot = errorDescription;
  tot += warningDescription;
  return CDA_wcsdup(tot.c_str());
}

iface::cellml_services::CUSESBootstrap*
CreateCUSESBootstrap(void)
{
  return new CDACUSESBootstrap();
}
