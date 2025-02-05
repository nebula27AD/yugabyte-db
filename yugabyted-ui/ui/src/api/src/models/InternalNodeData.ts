// tslint:disable
/**
 * Yugabyte Cloud
 * YugabyteDB as a Service
 *
 * The version of the OpenAPI document: v1
 * Contact: support@yugabyte.com
 *
 * NOTE: This class is auto generated by OpenAPI Generator (https://openapi-generator.tech).
 * https://openapi-generator.tech
 * Do not edit the class manually.
 */




/**
 * Internal node data
 * @export
 * @interface InternalNodeData
 */
export interface InternalNodeData  {
  /**
   * Name of the node (as recognized by platform)
   * @type {string}
   * @memberof InternalNodeData
   */
  name: string;
  /**
   * Private IP of the node
   * @type {string}
   * @memberof InternalNodeData
   */
  private_ip?: string;
  /**
   * Region the node belongs to
   * @type {string}
   * @memberof InternalNodeData
   */
  region: string;
}



