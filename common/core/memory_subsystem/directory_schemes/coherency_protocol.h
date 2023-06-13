#ifndef COHERENCYPROTOCOL_H_
#define COHERENCYPROTOCOL_H_

class CoherencyProtocol
{
   public:
      enum type_t
      {
         MSI,
         MESI,
         MESIF,
         MEI
      };
};

#endif /* COHERENCYPROTOCOL_H_ */
