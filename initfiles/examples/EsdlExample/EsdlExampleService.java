
package EsdlExample;
        
public class EsdlExampleService extends EsdlExampleServiceBase
{
    int counter=0;
    public EchoPersonInfoResponse EchoPersonInfo(EsdlContext context, EchoPersonInfoRequest request)
    {
        System.out.println("Method EchoPersonInfo of Service EsdlExample called!");
	EchoPersonInfoResponse  response = new EchoPersonInfoResponse();
        response.count = new Integer(++counter);
        response.Name = request.Name;
        response.Addresses = request.Addresses;
        return response;
    }
        
}
    
